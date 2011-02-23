// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "courgette/third_party/bsdiff.h"
#include "courgette/courgette.h"
#include "courgette/streams.h"


void PrintHelp() {
  fprintf(stderr,
    "Usage:\n"
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

std::string ReadOrFail(const std::wstring& file_name, const char* kind) {
#if defined(OS_WIN)
  FilePath file_path(file_name);
#else
  FilePath file_path(WideToASCII(file_name));
#endif
  int64 file_size = 0;
  if (!file_util::GetFileSize(file_path, &file_size))
    Problem("Can't read %s file.", kind);
  std::string buffer;
  buffer.reserve(static_cast<size_t>(file_size));
  if (!file_util::ReadFileToString(file_path, &buffer))
    Problem("Can't read %s file.", kind);
  return buffer;
}

void WriteSinkToFile(const courgette::SinkStream *sink,
                     const std::wstring& output_file) {
#if defined(OS_WIN)
  FilePath output_path(output_file);
#else
  FilePath output_path(WideToASCII(output_file));
#endif
  int count =
      file_util::WriteFile(output_path,
                           reinterpret_cast<const char*>(sink->Buffer()),
                           static_cast<int>(sink->Length()));
  if (count == -1)
    Problem("Can't write output.");
  if (static_cast<size_t>(count) != sink->Length())
    Problem("Incomplete write.");
}

void Disassemble(const std::wstring& input_file,
                 const std::wstring& output_file) {
  std::string buffer = ReadOrFail(input_file, "input");

  courgette::AssemblyProgram* program = NULL;
  const courgette::Status parse_status =
      courgette::ParseWin32X86PE(buffer.c_str(), buffer.length(), &program);

  if (parse_status != courgette::C_OK)
    Problem("Can't parse input.");

  courgette::EncodedProgram* encoded = NULL;
  const courgette::Status encode_status = Encode(program, &encoded);

  courgette::DeleteAssemblyProgram(program);

  if (encode_status != courgette::C_OK)
    Problem("Can't encode program.");

  courgette::SinkStreamSet sinks;

  const courgette::Status write_status =
    courgette::WriteEncodedProgram(encoded, &sinks);
  if (write_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");

  courgette::DeleteEncodedProgram(encoded);

  courgette::SinkStream sink;
  sinks.CopyTo(&sink);

  WriteSinkToFile(&sink, output_file);
}

void DisassembleAndAdjust(const std::wstring& program_file,
                          const std::wstring& model_file,
                          const std::wstring& output_file) {
  std::string program_buffer = ReadOrFail(program_file, "program");
  std::string model_buffer = ReadOrFail(model_file, "reference");

  courgette::AssemblyProgram* program = NULL;
  const courgette::Status parse_program_status =
      courgette::ParseWin32X86PE(program_buffer.c_str(),
                                 program_buffer.length(),
                                 &program);
  if (parse_program_status != courgette::C_OK)
    Problem("Can't parse program input.");

  courgette::AssemblyProgram* model = NULL;
  const courgette::Status parse_model_status =
      courgette::ParseWin32X86PE(model_buffer.c_str(),
                                 model_buffer.length(),
                                 &model);
  if (parse_model_status != courgette::C_OK)
    Problem("Can't parse model input.");

  const courgette::Status adjust_status = Adjust(*model, program);
  if (adjust_status != courgette::C_OK)
    Problem("Can't adjust program.");

  courgette::EncodedProgram* encoded = NULL;
  const courgette::Status encode_status = Encode(program, &encoded);

  courgette::DeleteAssemblyProgram(program);

  if (encode_status != courgette::C_OK)
    Problem("Can't encode program.");

  courgette::SinkStreamSet sinks;

  const courgette::Status write_status =
    courgette::WriteEncodedProgram(encoded, &sinks);
  if (write_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");

  courgette::DeleteEncodedProgram(encoded);

  courgette::SinkStream sink;
  sinks.CopyTo(&sink);

  WriteSinkToFile(&sink, output_file);
}

// Diffs two executable files, write a set of files for the diff, one file per
// stream of the EncodedProgram format.  Each file is the bsdiff between the
// original file's stream and the new file's stream.  This is completely
// uninteresting to users, but it is handy for seeing how much each which
// streams are contributing to the final file size.  Adjustment is optional.
void DisassembleAdjustDiff(const std::wstring& model_file,
                           const std::wstring& program_file,
                           const std::wstring& output_file_root,
                           bool adjust) {
  std::string model_buffer = ReadOrFail(model_file, "'old'");
  std::string program_buffer = ReadOrFail(program_file, "'new'");

  courgette::AssemblyProgram* model = NULL;
  const courgette::Status parse_model_status =
      courgette::ParseWin32X86PE(model_buffer.c_str(),
                                 model_buffer.length(),
                                 &model);
  if (parse_model_status != courgette::C_OK)
    Problem("Can't parse model input.");

  courgette::AssemblyProgram* program = NULL;
  const courgette::Status parse_program_status =
      courgette::ParseWin32X86PE(program_buffer.c_str(),
                                 program_buffer.length(),
                                 &program);
  if (parse_program_status != courgette::C_OK)
    Problem("Can't parse program input.");

  if (adjust) {
    const courgette::Status adjust_status = Adjust(*model, program);
    if (adjust_status != courgette::C_OK)
      Problem("Can't adjust program.");
  }

  courgette::EncodedProgram* encoded_program = NULL;
  const courgette::Status encode_program_status =
      Encode(program, &encoded_program);
  courgette::DeleteAssemblyProgram(program);
  if (encode_program_status != courgette::C_OK)
    Problem("Can't encode program.");

  courgette::EncodedProgram* encoded_model = NULL;
  const courgette::Status encode_model_status = Encode(model, &encoded_model);
  courgette::DeleteAssemblyProgram(model);
  if (encode_model_status != courgette::C_OK)
    Problem("Can't encode model.");

  courgette::SinkStreamSet program_sinks;
  const courgette::Status write_program_status =
    courgette::WriteEncodedProgram(encoded_program, &program_sinks);
  if (write_program_status != courgette::C_OK)
    Problem("Can't serialize encoded program.");
  courgette::DeleteEncodedProgram(encoded_program);

  courgette::SinkStreamSet model_sinks;
  const courgette::Status write_model_status =
    courgette::WriteEncodedProgram(encoded_model, &model_sinks);
  if (write_model_status != courgette::C_OK)
    Problem("Can't serialize encoded model.");
  courgette::DeleteEncodedProgram(encoded_model);

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
    courgette::BSDiffStatus status =
        courgette::CreateBinaryPatch(&old_source, &new_source, &patch_stream);
    if (status != courgette::OK) Problem("-xxx failed.");

    WriteSinkToFile(&patch_stream,
                    output_file_root + L"-" + UTF8ToWide(base::IntToString(i)));
  }
}

void Assemble(const std::wstring& input_file,
              const std::wstring& output_file) {
  std::string buffer = ReadOrFail(input_file, "input");

  courgette::SourceStreamSet sources;
  if (!sources.Init(buffer.c_str(), buffer.length()))
    Problem("Bad input file.");

  courgette::EncodedProgram* encoded = NULL;
  const courgette::Status read_status = ReadEncodedProgram(&sources, &encoded);
  if (read_status != courgette::C_OK)
    Problem("Bad encoded program.");

  courgette::SinkStream sink;

  const courgette::Status assemble_status = courgette::Assemble(encoded, &sink);
  if (assemble_status != courgette::C_OK)
    Problem("Can't assemble.");

  WriteSinkToFile(&sink, output_file);
}

void GenerateEnsemblePatch(const std::wstring& old_file,
                           const std::wstring& new_file,
                           const std::wstring& patch_file) {
  std::string old_buffer = ReadOrFail(old_file, "'old' input");
  std::string new_buffer = ReadOrFail(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer);
  new_stream.Init(new_buffer);

  courgette::SinkStream patch_stream;
  courgette::Status status =
      courgette::GenerateEnsemblePatch(&old_stream, &new_stream, &patch_stream);

  if (status != courgette::C_OK) Problem("-gen failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyEnsemblePatch(const std::wstring& old_file,
                        const std::wstring& patch_file,
                        const std::wstring& new_file) {
  // We do things a little differently here in order to call the same Courgette
  // entry point as the installer.  That entry point point takes file names and
  // returns an status code but does not output any diagnostics.
#if defined(OS_WIN)
  FilePath old_path(old_file);
  FilePath patch_path(patch_file);
  FilePath new_path(new_file);
#else
  FilePath old_path(WideToASCII(old_file));
  FilePath patch_path(WideToASCII(patch_file));
  FilePath new_path(WideToASCII(new_file));
#endif

  courgette::Status status =
    courgette::ApplyEnsemblePatch(old_path.value().c_str(),
                                  patch_path.value().c_str(),
                                  new_path.value().c_str());

  if (status == courgette::C_OK)
    return;

  // Diagnose the error.
  if (status == courgette::C_BAD_ENSEMBLE_MAGIC)
    Problem("Not a courgette patch");
  if (status == courgette::C_BAD_ENSEMBLE_VERSION)
    Problem("Wrong version patch");
  if (status == courgette::C_BAD_ENSEMBLE_HEADER)
    Problem("Corrupt patch");
  //  If we failed due to a missing input file, this will
  // print the message.
  std::string old_buffer = ReadOrFail(old_file, "'old' input");
  old_buffer.clear();
  std::string patch_buffer = ReadOrFail(patch_file, "'patch' input");
  patch_buffer.clear();

  // Non-input related errors:
  if (status == courgette::C_WRITE_OPEN_ERROR)
    Problem("Can't open output");
  if (status == courgette::C_WRITE_ERROR)
    Problem("Can't write output");

  Problem("-apply failed.");
}

void GenerateBSDiffPatch(const std::wstring& old_file,
                         const std::wstring& new_file,
                         const std::wstring& patch_file) {
  std::string old_buffer = ReadOrFail(old_file, "'old' input");
  std::string new_buffer = ReadOrFail(new_file, "'new' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream new_stream;
  old_stream.Init(old_buffer);
  new_stream.Init(new_buffer);

  courgette::SinkStream patch_stream;
  courgette::BSDiffStatus status =
      courgette::CreateBinaryPatch(&old_stream, &new_stream, &patch_stream);

  if (status != courgette::OK) Problem("-genbsdiff failed.");

  WriteSinkToFile(&patch_stream, patch_file);
}

void ApplyBSDiffPatch(const std::wstring& old_file,
                      const std::wstring& patch_file,
                      const std::wstring& new_file) {
  std::string old_buffer = ReadOrFail(old_file, "'old' input");
  std::string patch_buffer = ReadOrFail(patch_file, "'patch' input");

  courgette::SourceStream old_stream;
  courgette::SourceStream patch_stream;
  old_stream.Init(old_buffer);
  patch_stream.Init(patch_buffer);

  courgette::SinkStream new_stream;
  courgette::BSDiffStatus status =
      courgette::ApplyBinaryPatch(&old_stream, &patch_stream, &new_stream);

  if (status != courgette::OK) Problem("-applybsdiff failed.");

  WriteSinkToFile(&new_stream, new_file);
}

int main(int argc, const char* argv[]) {
  base::AtExitManager at_exit_manager;
  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  (void)logging::InitLogging(
      FILE_PATH_LITERAL("courgette.log"),
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  logging::SetMinLogLevel(logging::LOG_VERBOSE);

  bool cmd_dis = command_line.HasSwitch("dis");
  bool cmd_asm = command_line.HasSwitch("asm");
  bool cmd_disadj = command_line.HasSwitch("disadj");
  bool cmd_make_patch = command_line.HasSwitch("gen");
  bool cmd_apply_patch = command_line.HasSwitch("apply");
  bool cmd_make_bsdiff_patch = command_line.HasSwitch("genbsdiff");
  bool cmd_apply_bsdiff_patch = command_line.HasSwitch("applybsdiff");
  bool cmd_spread_1_adjusted = command_line.HasSwitch("gen1a");
  bool cmd_spread_1_unadjusted = command_line.HasSwitch("gen1u");

  // TODO(evanm): this whole file should use FilePaths instead of wstrings.
  std::vector<std::wstring> values;
  for (size_t i = 0; i < command_line.args().size(); ++i) {
#if defined(OS_WIN)
    values.push_back(command_line.args()[i]);
#else
    values.push_back(ASCIIToWide(command_line.args()[i]));
#endif
  }

  // '-repeat=N' is for debugging.  Running many iterations can reveal leaks and
  // bugs in cleanup.
  int repeat_count = 1;
  std::string repeat_switch = command_line.GetSwitchValueASCII("repeat");
  if (!repeat_switch.empty())
    if (!base::StringToInt(repeat_switch, &repeat_count))
      repeat_count = 1;

  if (cmd_dis + cmd_asm + cmd_disadj + cmd_make_patch + cmd_apply_patch +
      cmd_make_bsdiff_patch + cmd_apply_bsdiff_patch +
      cmd_spread_1_adjusted + cmd_spread_1_unadjusted
      != 1)
    UsageProblem(
        "Must have exactly one of:\n"
        "  -asm, -dis, -disadj, -gen or -apply, -genbsdiff or -applybsdiff.");

  while (repeat_count-- > 0) {
    if (cmd_dis) {
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
}
