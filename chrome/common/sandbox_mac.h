// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_MAC_H_
#define CHROME_COMMON_SANDBOX_MAC_H_
#pragma once

#include <string>

class FilePath;

namespace sandbox {

enum SandboxProcessType {
  SANDBOX_TYPE_FIRST_TYPE,  // Placeholder to ease iteration.

  SANDBOX_TYPE_RENDERER = SANDBOX_TYPE_FIRST_TYPE,

  // The worker processes uses the most restrictive sandbox which has almost
  // *everything* locked down. Only a couple of /System/Library/ paths and
  // some other very basic operations (e.g., reading metadata to allow
  // following symlinks) are permitted.
  SANDBOX_TYPE_WORKER,

  // Utility process is as restrictive as the worker process except full access
  // is allowed to one configurable directory.
  SANDBOX_TYPE_UTILITY,

  // Native Client sandbox for the user's untrusted code.
  SANDBOX_TYPE_NACL_LOADER,

  SANDBOX_AFTER_TYPE_LAST_TYPE,  // Placeholder to ease iteration.
};

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
void SandboxWarmup();

// Turns on the OS X sandbox for this process.
// |sandbox_type| - type of Sandbox to use.
// |allowed_dir| - directory to allow access to, currently the only sandbox
// profile that supports this is SANDBOX_TYPE_UTILITY .
//
// |allowed_dir| must be a "simple" string since it's placed as is in a regex
// i.e. it must not contain quotation characters, escaping or any characters
// that might have special meaning when blindly substituted into a regular
// expression - crbug.com/26492 .
// Returns true on success, false if an error occurred enabling the sandbox.
bool EnableSandbox(SandboxProcessType sandbox_type,
                   const FilePath& allowed_dir);

// Convert provided path into a "canonical" path matching what the Sandbox
// expects i.e. one without symlinks.
// This path is not necessarily unique e.g. in the face of hardlinks.
void GetCanonicalSandboxPath(FilePath* path);

// Exposed for testing.

// Class representing a substring of the sandbox profile tagged with its type.
class SandboxSubstring {
 public:
  enum SandboxSubstringType {
    PLAIN,    // Just a plain string, no escaping necessary.
    LITERAL,  // Escape for use in (literal ...) expression.
    REGEX,    // Escape for use in (regex ...) expression.
  };

  SandboxSubstring() {}

  explicit SandboxSubstring(const std::string& value)
      : value_(value),
        type_(PLAIN) {}

  SandboxSubstring(const std::string& value, SandboxSubstringType type)
      : value_(value),
        type_(type) {}

  const std::string& value() { return value_; }
  SandboxSubstringType type() { return type_; }

 private:
  std::string value_;
  SandboxSubstringType type_;
};

}  // namespace sandbox

#endif  // CHROME_COMMON_SANDBOX_MAC_H_
