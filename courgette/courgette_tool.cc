// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "courgette/assembly_program.h"
#include "courgette/courgette.h"
#include "courgette/encoded_program.h"
#include "courgette/program_detector.h"
#include "courgette/streams.h"
#include "courgette/third_party/bsdiff/bsdiff.h"

void PrintHelp() {
  fprintf(stderr,
    "Usage:\n"
    "  courgette -supported <executable_file>\n"
    "  courgette -dis <executable_file> <binary_assembly_file>\n"
    "  courgette -asm <binary_assembly_file> <executable_file>\n"
    "  courgette -disadj <executable_file> <reference> <binary_assembly_file>\n"
    "  courgette -gen <v1> <v2> <patch>\n"
    "  courgette -apply <v1> <patch> <v2>\n"
    "\n");
}

void UsageProblem(const char* message) {
  fprintf(stderr, "%s", message);
  fprintf(stderr, "\n");
  PrintHelp();
  exit(1);
}

void Problem(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

// A file reader that calls Problem() on failure.
class BufferedFileReader {
 public:
  BufferedFileReader(const base::FilePath& file_name, const char* kind) {
    if (!buffer_.Initialize(file_name))
      Problem("Can't read %s file.", kind);
  }
  const uint8_t* data() const { return buffer_.data(); }
  size_t length() const { return buffer_.length(); }

 private:
  base::MemoryMappedFile buffer_;
};

void WriteSinkToFile(const courgette::SinkStream *sink,
                     const base::FilePath& output_file) {
  int count =
      base::WriteFile(output_file,
                           reinterpret_cast<const char*>(sink->Buffer()),
                           static_cast<int>(sink->Length()));
  if (count == -1)
    Problem("Can't write output.");
  if (static_cast<size_t>(count) != sink->Length())
    Problem("Incomplete write.");
}

void Disassemble(const base::FilePath& input_file,
                 const base::FilePath& output_file) {
  BufferedFileReader buffer(input_file, "input");

  std::unique_ptr<courgette::AssemblyProgram> program;
  const courgette::Status parse_status = courgette::ParseDetectedExecutable(
      buffer.data(), buffer.length(), &program);
  if (parse_status != courgette::C_OK)
    Problem("Can't parse input (code = %d).", parse_status);

  std::unique_ptr<courgette::EncodedProgram> encoded;
  const courgette::Status encode_status = Encode(*program, &encoded);
  if (encode_status != courgette::C_OK)
    Problem("Can't encode program.");

  program.reset();

  courgette::SinkStreamSet sinks;
  const courgette::Status write_status =
    courgette::WriteEncodedProgram(encoded.get(), &sinks);
  if (write_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");

  encoded.reset();

  courgette::SinkStream sink;
  if (!sinks.CopyTo(&sink))
    Problem("Can't combine serialized encoded program streams.");

  WriteSinkToFile(&sink, output_file);
}

bool Supported(const base::FilePath& input_file) {
  bool result = false;

  BufferedFileReader buffer(input_file, "input");

  courgette::ExecutableType type;
  size_t detected_length;

  DetectExecutableType(buffer.data(), buffer.length(), &type, &detected_length);

  // If the detection fails, we just fall back on UNKNOWN
  std::string format = "Unsupported";

  switch (type)
  {
    case courgette::EXE_UNKNOWN:
      break;

    case courgette::EXE_WIN_32_X86:
      format = "Windows 32 PE";
      result = true;
      break;

    case courgette::EXE_ELF_32_X86:
      format = "ELF 32 X86";
      result = true;
      break;

    case courgette::EXE_ELF_32_ARM:
      format = "ELF 32 ARM";
      result = true;
      break;

    case courgette::EXE_WIN_32_X64:
      format = "Windows 64 PE";
      result = true;
      break;
  }

  printf("%s Executable\n", format.c_str());
  return result;
}

void DisassembleAndAdjust(const base::FilePath& program_file,
                          const base::FilePath& model_file,
                          const base::FilePath& output_file) {
  BufferedFileReader program_buffer(program_file, "program");
  BufferedFileReader model_buffer(model_file, "model");

  std::unique_ptr<courgette::AssemblyProgram> program;
  const courgette::Status parse_program_status =
      courgette::ParseDetectedExecutable(program_buffer.data(),
                                         program_buffer.length(), &program);
  if (parse_program_status != courgette::C_OK)
    Problem("Can't parse program input (code = %d).", parse_program_status);

  std::unique_ptr<courgette::AssemblyProgram> model;
  const courgette::Status parse_model_status =
      courgette::ParseDetectedExecutable(model_buffer.data(),
                                         model_buffer.length(), &model);
  if (parse_model_status != courgette::C_OK)
    Problem("Can't parse model input (code = %d).", parse_model_status);

  const courgette::Status adjust_status = Adjust(*model, program.get());
  if (adjust_status != courgette::C_OK)
    Problem("Can't adjust program.");

  model.reset();

  std::unique_ptr<courgette::EncodedProgram> encoded;
  const courgette::Status encode_status = Encode(*program, &encoded);
  if (encode_status != courgette::C_OK)
    Problem("Can't encode program.");

  program.reset();

  courgette::SinkStreamSet sinks;
  const courgette::Status write_status =
    courgette::WriteEncodedProgram(encoded.get(), &sinks);
  if (write_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");

  encoded.reset();

  courgette::SinkStream sink;
  if (!sinks.CopyTo(&sink))
    Problem("Can't combine serialized encoded program streams.");

  WriteSinkToFile(&sink, output_file);
}

// Diffs two executable files, write a set of files for the diff, one file per
// stream of the EncodedProgram format.  Each file is the bsdiff between the
// original file's stream and the new file's stream.  This is completely
// uninteresting to users, but it is handy for seeing how much each which
// streams are contributing to the final file size.  Adjustment is optional.
void DisassembleAdjustDiff(const base::FilePath& model_file,
                           const base::FilePath& program_file,
                           const base::FilePath& output_file_root,
                           bool adjust) {
  BufferedFileReader model_buffer(model_file, "old");
  BufferedFileReader program_buffer(program_file, "new");

  std::unique_ptr<courgette::AssemblyProgram> model;
  const courgette::Status parse_model_status =
      courgette::ParseDetectedExecutable(model_buffer.data(),
                                         model_buffer.length(), &model);
  if (parse_model_status != courgette::C_OK)
    Problem("Can't parse model input (code = %d).", parse_model_status);

  std::unique_ptr<courgette::AssemblyProgram> program;
  const courgette::Status parse_program_status =
      courgette::ParseDetectedExecutable(program_buffer.data(),
                                         program_buffer.length(), &program);
  if (parse_program_status != courgette::C_OK)
    Problem("Can't parse program input (code = %d).", parse_program_status);

  if (adjust) {
    const courgette::Status adjust_status = Adjust(*model, program.get());
    if (adjust_status != courgette::C_OK)
      Problem("Can't adjust program.");
  }

  std::unique_ptr<courgette::EncodedProgram> encoded_program;
  const courgette::Status encode_program_status =
      Encode(*program, &encoded_program);
  if (encode_program_status != courgette::C_OK)
    Problem("Can't encode program.");

  program.reset();

  std::unique_ptr<courgette::EncodedProgram> encoded_model;
  const courgette::Status encode_model_status = Encode(*model, &encoded_model);
  if (encode_model_status != courgette::C_OK)
    Problem("Can't encode model.");

  model.reset();

  courgette::SinkStreamSet program_sinks;
  const courgette::Status write_program_status =
    courgette::WriteEncodedProgram(encoded_program.get(), &program_sinks);
  if (write_program_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");

  encoded_program.reset();

  courgette::SinkStreamSet model_sinks;
  const courgette::Status write_model_status =
    courgette::WriteEncodedProgram(encoded_model.get(), &model_sinks);
  if (write_model_status != courgette::C_OK)
    Problem("Can't serialize encoded model.");

  encoded_model.reset();

  courgette::SinkStream empty_sink;
  for (int i = 0;  ; ++i) {
    courgette::SinkStream* old_stream = model_sinks.stream(i);
    courgette::SinkStream* new_stream = program_sinks.stream(i);
    if (old_stream == NULL && new_stream == NULL)
      break;

    courgette::SourceStream old_source;
    courgette::SourceStream new_source;
    old_source.Init(old_stream ? *old_stream : empty_sink);
    new_source.Init(new_stream ? *new_stream : empty_sink);
    courgette::SinkStream patch_stream;
    bsdiff::BSDiffStatus status =
        bsdiff::CreateBinaryPatch(&old_source, &new_source, &patch_stream);
    if (status != bsdiff::OK)
      Problem("-xxx failed.");

    std::string append = std::string("-") + base::IntToString(i);

    WriteSinkToFile(&patch_stream,
                    output_file_root.InsertBeforeExtensionASCII(append));
  }
}

void Assemble(const base::FilePath& input_file,
              const base::FilePath& output_file) {
  BufferedFileReader buffer(input_file, "input");

  courgette::SourceStreamSet sources;
  if (!sources.Init(buffer.data(), buffer.length()))
    Problem("Bad input file.");

  std::unique_ptr<courgette::EncodedProgram> encoded;
  const courgette::Status read_status =
      courgette::ReadEncodedProgram(&sources, &encoded);
  if (read_status != courgette::C_OK)
    Problem("Bad encoded program.");

  courgette::SinkStream sink;

  const courgette::Status assemble_status =
      courgette::Assemble(encoded.get(), &sink);
  if (assemble_status != courgette::C_OK)
    Problem("Can't assemble.");

  WriteSinkToFile(&sink, output_file);
}

void GenerateEnsemblePatch(const base::FilePath& old_file,
                           const base::FilePath& new_file,
                           const base::FilePath& patch_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader new_buffer(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  new_stream.Init(new_buffer.data(), new_buffer.length());

  courgette::SinkStream patch_stream;
  courgette::Status status =
      courgette::GenerateEnsemblePatch(&old_stream, &new_stream, &patch_stream);

  if (status != courgette::C_OK)
    Problem("-gen failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyEnsemblePatch(const base::FilePath& old_file,
                        const base::FilePath& patch_file,
                        const base::FilePath& new_file) {
  // We do things a little differently here in order to call the same Courgette
  // entry point as the installer.  That entry point point takes file names and
  // returns an status code but does not output any diagnostics.

  courgette::Status status =
      courgette::ApplyEnsemblePatch(old_file.value().c_str(),
                                    patch_file.value().c_str(),
                                    new_file.value().c_str());

  if (status == courgette::C_OK)
    return;

  // Diagnose the error.
  switch (status) {
    case courgette::C_BAD_ENSEMBLE_MAGIC:
      Problem("Not a courgette patch");
      break;

    case courgette::C_BAD_ENSEMBLE_VERSION:
      Problem("Wrong version patch");
      break;

    case courgette::C_BAD_ENSEMBLE_HEADER:
      Problem("Corrupt patch");
      break;

    case courgette::C_DISASSEMBLY_FAILED:
      Problem("Disassembly failed (could be because of memory issues)");
      break;

    case courgette::C_STREAM_ERROR:
      Problem("Stream error (likely out of memory or disk space)");
      break;

    default:
      break;
  }

  // If we failed due to a missing input file, this will print the message.
  { BufferedFileReader old_buffer(old_file, "'old' input"); }
  { BufferedFileReader patch_buffer(patch_file, "'patch' input"); }

  // Non-input related errors:
  if (status == courgette::C_WRITE_OPEN_ERROR)
    Problem("Can't open output");
  if (status == courgette::C_WRITE_ERROR)
    Problem("Can't write output");

  Problem("-apply failed.");
}

void GenerateBSDiffPatch(const base::FilePath& old_file,
                         const base::FilePath& new_file,
                         const base::FilePath& patch_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader new_buffer(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  new_stream.Init(new_buffer.data(), new_buffer.length());

  courgette::SinkStream patch_stream;
  bsdiff::BSDiffStatus status =
      bsdiff::CreateBinaryPatch(&old_stream, &new_stream, &patch_stream);

  if (status != bsdiff::OK)
    Problem("-genbsdiff failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyBSDiffPatch(const base::FilePath& old_file,
                      const base::FilePath& patch_file,
                      const base::FilePath& new_file) {
  BufferedFileReader old_buffer(old_file, "'old' input");
  BufferedFileReader patch_buffer(patch_file, "'patch' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream patch_stream;
  old_stream.Init(old_buffer.data(), old_buffer.length());
  patch_stream.Init(patch_buffer.data(), patch_buffer.length());

  courgette::SinkStream new_stream;
  bsdiff::BSDiffStatus status =
      bsdiff::ApplyBinaryPatch(&old_stream, &patch_stream, &new_stream);

  if (status != bsdiff::OK)
    Problem("-applybsdiff failed.");

  WriteSinkToFile(&new_stream, new_file);
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  base::CommandLine::Init(argc, argv);
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_ALL;
  settings.log_file = FILE_PATH_LITERAL("courgette.log");
  (void)logging::InitLogging(settings);
  logging::SetMinLogLevel(logging::LOG_VERBOSE);

  bool cmd_sup = command_line.HasSwitch("supported");
  bool cmd_dis = command_line.HasSwitch("dis");
  bool cmd_asm = command_line.HasSwitch("asm");
  bool cmd_disadj = command_line.HasSwitch("disadj");
  bool cmd_make_patch = command_line.HasSwitch("gen");
  bool cmd_apply_patch = command_line.HasSwitch("apply");
  bool cmd_make_bsdiff_patch = command_line.HasSwitch("genbsdiff");
  bool cmd_apply_bsdiff_patch = command_line.HasSwitch("applybsdiff");
  bool cmd_spread_1_adjusted = command_line.HasSwitch("gen1a");
  bool cmd_spread_1_unadjusted = command_line.HasSwitch("gen1u");

  std::vector<base::FilePath> values;
  const base::CommandLine::StringVector& args = command_line.GetArgs();
  for (size_t i = 0; i < args.size(); ++i) {
    values.push_back(base::FilePath(args[i]));
  }

  // '-repeat=N' is for debugging.  Running many iterations can reveal leaks and
  // bugs in cleanup.
  int repeat_count = 1;
  std::string repeat_switch = command_line.GetSwitchValueASCII("repeat");
  if (!repeat_switch.empty())
    if (!base::StringToInt(repeat_switch, &repeat_count))
      repeat_count = 1;

  if (cmd_sup + cmd_dis + cmd_asm + cmd_disadj + cmd_make_patch +
      cmd_apply_patch + cmd_make_bsdiff_patch + cmd_apply_bsdiff_patch +
      cmd_spread_1_adjusted + cmd_spread_1_unadjusted
      != 1)
    UsageProblem(
        "Must have exactly one of:\n"
        "  -supported -asm, -dis, -disadj, -gen or -apply, -genbsdiff"
        " or -applybsdiff.");

  while (repeat_count-- > 0) {
    if (cmd_sup) {
      if (values.size() != 1)
        UsageProblem("-supported <executable_file>");
      return !Supported(values[0]);
    } else if (cmd_dis) {
        if (values.size() != 2)
          UsageProblem("-dis <executable_file> <courgette_file>");
        Disassemble(values[0], values[1]);
    } else if (cmd_asm) {
      if (values.size() != 2)
        UsageProblem("-asm <courgette_file_input> <executable_file_output>");
      Assemble(values[0], values[1]);
    } else if (cmd_disadj) {
      if (values.size() != 3)
        UsageProblem("-disadj <executable_file> <model> <courgette_file>");
      DisassembleAndAdjust(values[0], values[1], values[2]);
    } else if (cmd_make_patch) {
      if (values.size() != 3)
        UsageProblem("-gen <old_file> <new_file> <patch_file>");
      GenerateEnsemblePatch(values[0], values[1], values[2]);
    } else if (cmd_apply_patch) {
      if (values.size() != 3)
        UsageProblem("-apply <old_file> <patch_file> <new_file>");
      ApplyEnsemblePatch(values[0], values[1], values[2]);
    } else if (cmd_make_bsdiff_patch) {
      if (values.size() != 3)
        UsageProblem("-genbsdiff <old_file> <new_file> <patch_file>");
      GenerateBSDiffPatch(values[0], values[1], values[2]);
    } else if (cmd_apply_bsdiff_patch) {
      if (values.size() != 3)
        UsageProblem("-applybsdiff <old_file> <patch_file> <new_file>");
      ApplyBSDiffPatch(values[0], values[1], values[2]);
    } else if (cmd_spread_1_adjusted || cmd_spread_1_unadjusted) {
      if (values.size() != 3)
        UsageProblem("-gen1[au] <old_file> <new_file> <patch_files_root>");
      DisassembleAdjustDiff(values[0], values[1], values[2],
                            cmd_spread_1_adjusted);
    } else {
      UsageProblem("No operation specified");
    }
  }

  return 0;
}
