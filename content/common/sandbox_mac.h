// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_MAC_H_
#define CONTENT_COMMON_SANDBOX_MAC_H_

#include <map>
#include <string>

#include "base/containers/hash_tables.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/sandbox_type.h"

namespace base {
class FilePath;
}

namespace content {

class CONTENT_EXPORT Sandbox {
 public:

  // Warm up System APIs that empirically need to be accessed before the
  // sandbox is turned on. |sandbox_type| is the type of sandbox to warm up.
  // Valid |sandbox_type| values are defined by the enum SandboxType, or can be
  // defined by the embedder via
  // ContentClient::GetSandboxProfileForProcessType().
  static void SandboxWarmup(int sandbox_type);

  // Turns on the OS X sandbox for this process.
  // |sandbox_type| - type of Sandbox to use. See SandboxWarmup() for legal
  // values.
  // |allowed_dir| - directory to allow access to, currently the only sandbox
  // profile that supports this is SANDBOX_TYPE_UTILITY .
  //
  // Returns true on success, false if an error occurred enabling the sandbox.
  static bool EnableSandbox(int sandbox_type,
                            const base::FilePath& allowed_dir);

  // Returns true if the sandbox has been enabled for the current process.
  static bool SandboxIsCurrentlyActive();

  // Convert provided path into a "canonical" path matching what the Sandbox
  // expects i.e. one without symlinks.
  // This path is not necessarily unique e.g. in the face of hardlinks.
  static base::FilePath GetCanonicalSandboxPath(const base::FilePath& path);

  static const char* kSandboxBrowserPID;
  static const char* kSandboxBundlePath;
  static const char* kSandboxChromeBundleId;
  static const char* kSandboxComponentPath;
  static const char* kSandboxDisableDenialLogging;
  static const char* kSandboxEnableLogging;
  static const char* kSandboxHomedirAsLiteral;
  static const char* kSandboxLoggingPathAsLiteral;
  static const char* kSandboxOSVersion;
  static const char* kSandboxPermittedDir;

  // TODO(kerrnel): this is only for the legacy sandbox.
  static const char* kSandboxElCapOrLater;

 private:
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, StringEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, RegexEscape);
  FRIEND_TEST_ALL_PREFIXES(MacDirAccessSandboxTest, SandboxAccess);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Sandbox);
};

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_MAC_H_
