// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/subresource_filter/tools/ruleset_converter/rule_stream.h"
#include "components/subresource_filter/tools/ruleset_converter/ruleset_format.h"

namespace {

const char kSwitchInputFiles[] = "input_files";
const char kSwitchOutputFile[] = "output_file";

const char kSwitchOutputFileUrl[] = "output_file_url";
const char kSwitchOutputFileCss[] = "output_file_css";

const char kSwitchInputFormat[] = "input_format";
const char kSwitchOutputFormat[] = "output_format";

const char kSwitchChromeVersion[] = "chrome_version";

const char kHelpMsg[] = R"(
  ruleset_converter [--input_format=<format>] --output_format=<format>
  --input_files=<path1>[:<path2>...]
  (--output_file=<path> | [--output_file_url=<path>] [--output_file_css=<path>)
  [--chrome_version=<version>]

  ruleset_converter is a utility for converting subresource_filter rulesets
  across multiple formats:

  * --input_files: Colon-separated list of input files with rules. The files
     are processed in the order of declaration

  * --output_file: The file to output the rules. Either this option or at least
    one of the --output_file_url|--output_file_css should be specified.

  * --output_file_url: The file to output URL rules. If equal to
    --output_file_css, the results are merged.

  *  --output_file_css: The file to output CSS rules. See --output_file and
     --output_file_url for details.

  * --input_format: The format of the input file(s). One of
    {filter-list, proto, unindexed-ruleset}

  * --output_format: The format of the output file(s). See --input_format for
    available formats.

  * --chrome_version: The earliest version of Chrome that the produced ruleset
    needs to be compatible with. Currently one of 54, 59, or 0
    (not Chrome-specific). Defaults to the maximum (i.e. 59).
)";

void PrintHelp() {
  printf("%s\n\n", kHelpMsg);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::CommandLine::Init(argc, argv);
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  if (command_line.GetArgs().size() != 0U) {
    PrintHelp();
    return 1;
  }

  if (!command_line.HasSwitch(kSwitchInputFiles)) {
    std::fprintf(stderr, "--input_files flag is not specified.\n");
    PrintHelp();
    return 1;
  }

  if (command_line.GetSwitchValueASCII(kSwitchOutputFile).empty() &&
      command_line.GetSwitchValueASCII(kSwitchOutputFileUrl).empty() &&
      command_line.GetSwitchValueASCII(kSwitchOutputFileCss).empty()) {
    std::fprintf(stderr,
                 "Either --output_file, or at least of one "
                 "--output_file_url|--output_file_css should be specified.\n");
    PrintHelp();
    return 1;
  }

  const subresource_filter::RulesetFormat input_format =
      command_line.HasSwitch(kSwitchInputFormat)
          ? subresource_filter::ParseFlag(
                command_line.GetSwitchValueASCII(kSwitchInputFormat))
          : subresource_filter::RulesetFormat::kFilterList;
  if (input_format == subresource_filter::RulesetFormat::kUndefined) {
    std::fprintf(stderr, "Input format is not defined.\n");
    return 1;
  }

  const subresource_filter::RulesetFormat output_format =
      subresource_filter::ParseFlag(
          command_line.GetSwitchValueASCII(kSwitchOutputFormat));
  if (output_format == subresource_filter::RulesetFormat::kUndefined) {
    std::fprintf(stderr, "Output format is not defined.\n");
    return 1;
  }

  int chrome_version = 59;
  if (command_line.HasSwitch(kSwitchChromeVersion)) {
    if (!base::StringToInt(
            command_line.GetSwitchValueASCII(kSwitchChromeVersion),
            &chrome_version)) {
      fprintf(stderr, "Unable to parse chrome version");
      return 1;
    }
  }
  if (chrome_version != 0 && chrome_version != 54 && chrome_version != 59) {
    std::fprintf(stderr, "chrome_version should be in {0, 54, 59}.\n");
    return 1;
  }

  // Vet the input paths.
  base::CommandLine::StringType inputs =
      command_line.GetSwitchValueNative(kSwitchInputFiles);
  std::vector<base::FilePath> input_paths;
#if defined(OS_WIN)
  base::StringPiece16 separator(base::ASCIIToUTF16(":"));
#else
  base::StringPiece separator(":");
#endif
  for (const auto& piece :
       base::SplitStringPiece(inputs, separator, base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    base::FilePath path(piece);
    if (!base::PathExists(path)) {
      std::fprintf(stderr, "Input path does not exist\n");
      return 1;
    }
    input_paths.push_back(path);
  }

  // Create output stream(s).
  std::unique_ptr<subresource_filter::RuleOutputStream> primary_output;
  std::unique_ptr<subresource_filter::RuleOutputStream> secondary_output;
  subresource_filter::RuleOutputStream* css_rules_output = nullptr;

  base::FilePath primary_filename =
      command_line.GetSwitchValuePath(kSwitchOutputFile);
  const bool single_output = !primary_filename.empty();
  if (!single_output)
    primary_filename = command_line.GetSwitchValuePath(kSwitchOutputFileUrl);

  if (!primary_filename.empty()) {
    if (!base::DirectoryExists(primary_filename.DirName())) {
      std::fprintf(stderr, "Output directory does not exist\n");
      return 1;
    }
    primary_output = subresource_filter::RuleOutputStream::Create(
        std::make_unique<std::ofstream>(primary_filename.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::out),
        output_format);
  }

  base::FilePath secondary_filename =
      command_line.GetSwitchValuePath(kSwitchOutputFileCss);
  if (single_output || secondary_filename == primary_filename) {
    css_rules_output = primary_output.get();
  } else if (!secondary_filename.empty()) {
    if (!base::DirectoryExists(secondary_filename.DirName())) {
      std::fprintf(stderr, "Output directory does not exist\n");
      return 1;
    }
    secondary_output = subresource_filter::RuleOutputStream::Create(
        std::make_unique<std::ofstream>(secondary_filename.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::out),
        output_format);
    css_rules_output = secondary_output.get();
  }

  // Iterate through input files and stream them to the outputs.
  for (const auto& path : input_paths) {
    auto input_stream = subresource_filter::RuleInputStream::Create(
        std::make_unique<std::ifstream>(path.AsUTF8Unsafe(),
                                        std::ios::binary | std::ios::in),
        input_format);
    CHECK(input_stream);
    CHECK(subresource_filter::TransferRules(input_stream.get(),
                                            primary_output.get(),
                                            css_rules_output, chrome_version));
  }
  if (primary_output)
    CHECK(primary_output->Finish());
  if (secondary_output)
    CHECK(secondary_output->Finish());
  return 0;
}
