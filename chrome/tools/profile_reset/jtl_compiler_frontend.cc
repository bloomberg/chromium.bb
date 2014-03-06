// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple command-line compiler for JTL (JSON Traversal Language).
//
// Translates rules from a text-based, human-readable format to an easy-to-parse
// byte-code format, which then can be interpreted by JtlInterpreter.
//
// Example usage:
//   jtl_compiler --input=blah.txt --hash-seed="foobar" --output=blah.dat

#include <iostream>
#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/tools/profile_reset/jtl_compiler.h"

namespace {

// Command-line argument name: path to the input text-based JTL source code.
const char kInputPath[] = "input";

// Command-line argument name: path to the output byte-code.
const char kOutputPath[] = "output";

// Command-line argument name: the hash seed to use.
const char kHashSeed[] = "hash-seed";

// Error codes.
const char kMismatchedDoubleQuotes[] = "Mismatched double-quotes before EOL.";
const char kParsingError[] = "Parsing error. Input is ill-formed.";
const char kArgumentCountError[] = "Wrong number of arguments for operation.";
const char kArgumentTypeError[] = "Wrong argument type(s) for operation.";
const char kArgumentValueError[] = "Wrong argument value(s) for operation.";
const char kUnknownOperationError[] = "No operation by this name.";
const char kUnknownError[] = "Unknown error.";

const char* ResolveErrorCode(JtlCompiler::CompileError::ErrorCode code) {
  switch (code) {
    case JtlCompiler::CompileError::MISMATCHED_DOUBLE_QUOTES:
      return kMismatchedDoubleQuotes;
    case JtlCompiler::CompileError::PARSING_ERROR:
      return kParsingError;
    case JtlCompiler::CompileError::INVALID_ARGUMENT_COUNT:
      return kArgumentCountError;
    case JtlCompiler::CompileError::INVALID_ARGUMENT_TYPE:
      return kArgumentTypeError;
    case JtlCompiler::CompileError::INVALID_ARGUMENT_VALUE:
      return kArgumentValueError;
    case JtlCompiler::CompileError::INVALID_OPERATION_NAME:
      return kUnknownOperationError;
    default:
      return kUnknownError;
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  CommandLine::Init(argc, argv);
  CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kInputPath) || !cmd_line->HasSwitch(kHashSeed) ||
      !cmd_line->HasSwitch(kOutputPath)) {
    std::cerr << "Usage: " << argv[0] << " <required switches>" << std::endl;
    std::cerr << "\nRequired switches are:" << std::endl;
    std::cerr << "  --" << kInputPath << "=<file>"
              << "\t\tPath to the input text-based JTL source code."
              << std::endl;
    std::cerr << "  --" << kOutputPath << "=<file>"
              << "\t\tPath to the output byte-code." << std::endl;
    std::cerr << "  --" << kHashSeed << "=<value>"
              << "\t\tThe hash seed to use." << std::endl;
    return -1;
  }

  base::FilePath source_code_path =
      MakeAbsoluteFilePath(cmd_line->GetSwitchValuePath(kInputPath));
  std::string source_code;
  if (!base::ReadFileToString(source_code_path, &source_code)) {
    std::cerr << "ERROR: Cannot read input file." << std::endl;
    return -3;
  }

  std::string bytecode;
  JtlCompiler::CompileError error;
  std::string hash_seed = cmd_line->GetSwitchValueASCII(kHashSeed);
  if (!JtlCompiler::Compile(source_code, hash_seed, &bytecode, &error)) {
    std::cerr << "COMPILE ERROR: " << ResolveErrorCode(error.error_code)
              << std::endl;
    std::cerr << "  Line number: " << (error.line_number + 1) << std::endl;
    std::cerr << "  Context: " << (error.context.size() > 63
                                       ? error.context.substr(0, 60) + "..."
                                       : error.context) << std::endl;
    return -2;
  }

  base::FilePath bytecode_path =
      MakeAbsoluteFilePath(cmd_line->GetSwitchValuePath(kOutputPath));
  int bytes_written =
      base::WriteFile(cmd_line->GetSwitchValuePath(kOutputPath),
                           bytecode.data(),
                           static_cast<int>(bytecode.size()));
  if (bytes_written != static_cast<int>(bytecode.size())) {
    std::cerr << "ERROR: Cannot write output file." << std::endl;
    return -3;
  }

  return 0;
}
