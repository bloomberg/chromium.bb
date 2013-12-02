// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_PROFILE_RESET_JTL_COMPILER_H_
#define CHROME_TOOLS_PROFILE_RESET_JTL_COMPILER_H_

#include <string>

#include "base/basictypes.h"

// Compiles text-based JTL source code into JTL byte-code.
//
// For an overview of JTL (JSON Traversal Language), and the exhaustive list of
// instructions, please see "chrome/browser/profile_resetter/jtl_foundation.h".
//
// The text-based JTL syntax itself much resembles C/C++. A program consists of
// zero or more sentences. Each sentence is terminated by a semi-colon (;), and
// is composed of *one* or more operations, separated by periods (.).
//
// Each operation resembles a C/C++ function call and consists of an instruction
// name, and an optional argument list, which takes Boolean values and/or string
// literals. The text-based instruction names are defined in "jtl_compiler.cc".
//
// Whitespace does not matter, except for inside string literals. C++-style,
// double-slash-introduced comments are also supported.
//
// Example source code:
//
//   // Store "x"=true if path "foo.bar" is found.
//   go("foo").go("bar").store_bool("x", true);
//
//   // Store "y"="1" if the above value is set.
//   compare_stored_bool("x", true, false).store_hash("y", "1");
//
class JtlCompiler {
 public:
  struct CompileError {
    enum ErrorCode {
      ERROR_NONE,
      MISMATCHED_DOUBLE_QUOTES,
      PARSING_ERROR,
      INVALID_OPERATION_NAME,
      INVALID_ARGUMENT_COUNT,
      INVALID_ARGUMENT_TYPE,
      INVALID_ARGUMENT_VALUE
    };

    CompileError() : line_number(0), error_code(ERROR_NONE) {}
    CompileError(size_t line_number,
                 const std::string& context,
                 ErrorCode error_code)
        : line_number(line_number), context(context), error_code(error_code) {}

    size_t line_number;  // 0-based.
    std::string context;
    ErrorCode error_code;
  };

  // Compiles text-based JTL source code contained in |source_code| into JTL
  // byte-code to |output_bytecode|. Variable, node names, and string literals
  // will be hashed using the seed in |hash_seed|.
  // On success, returns true. Otherwise, returns false and fills |error| with
  // more information (if it is non-NULL).
  static bool Compile(const std::string& source_code,
                      const std::string& hash_seed,
                      std::string* output_bytecode,
                      CompileError* error);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JtlCompiler);
};

#endif  // CHROME_TOOLS_PROFILE_RESET_JTL_COMPILER_H_
