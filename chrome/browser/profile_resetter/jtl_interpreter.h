// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_JTL_INTERPRETER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_JTL_INTERPRETER_H_

#include <string>

#include "base/basictypes.h"
#include "base/values.h"

// Executes a JTL program on a given dictionary.
//
// JTL (Json Traversal Language) programs are defined in jtl_foundation.h
class JtlInterpreter {
 public:
  enum Result {
    OK,
    PARSE_ERROR,
    RUNTIME_ERROR,
    RESULT_MAX,
  };

  // |hasher_seed| is a value used in jtl_foundation::Hasher. All node names,
  // strings, integers and doubles are hashed before being compared to hash
  // values listed in |program|.
  // |program| is a byte array containing a JTL program.
  // |input| is a dictionary on which the program is evaluated.
  JtlInterpreter(const std::string& hasher_seed,
                 const std::string& program,
                 const base::DictionaryValue* input);
  ~JtlInterpreter();

  void Execute();

  Result result() const { return result_; }
  const base::DictionaryValue* working_memory() const {
    return working_memory_.get();
  }
  bool GetOutputBoolean(const std::string& unhashed_key, bool* output) const;
  bool GetOutputString(const std::string& unhashed_key,
                       std::string* output) const;

  // Generates a checksum of the loaded program, defined as the first 3 bytes of
  // the program's SHA-256 hash interpreted as a big-endian integer.
  int CalculateProgramChecksum() const;

 private:
  // Input.
  std::string hasher_seed_;
  std::string program_;
  const base::DictionaryValue* input_;
  // Output.
  scoped_ptr<base::DictionaryValue> working_memory_;
  Result result_;

  DISALLOW_COPY_AND_ASSIGN(JtlInterpreter);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_JTL_INTERPRETER_H_
