// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"

class ComponentInstaller;
class ComponentPatcher;

// Deserializes the CRX manifest. The top level must be a dictionary.
scoped_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path);

// In charge of unpacking the component CRX package and verifying that it is
// well formed and the cryptographic signature is correct. If there is no
// error the component specific installer will be invoked to proceed with
// the component installation or update.
//
// This class should be used only by the component updater. It is inspired
// and overlaps with code in the extension's SandboxedUnpacker.
// The main differences are:
// - The public key hash is full SHA256.
// - Does not use a sandboxed unpacker. A valid component is fully trusted.
// - The manifest can have different attributes and resources are not
//   transcoded.
class ComponentUnpacker {
 public:
  // Possible error conditions.
  // Add only to the bottom of this enum; the order must be kept stable.
  enum Error {
    kNone,
    kInvalidParams,
    kInvalidFile,
    kUnzipPathError,
    kUnzipFailed,
    kNoManifest,
    kBadManifest,
    kBadExtension,
    kInvalidId,
    kInstallerError,
    kIoError,
    kDeltaVerificationFailure,
    kDeltaBadCommands,
    kDeltaUnsupportedCommand,
    kDeltaOperationFailure,
    kDeltaPatchProcessFailure,
    kDeltaMissingExistingFile,
    kFingerprintWriteFailed,
  };
  // Unpacks, verifies and calls the installer. |pk_hash| is the expected
  // public key SHA256 hash. |path| is the current location of the CRX.
  ComponentUnpacker(const std::vector<uint8>& pk_hash,
                    const base::FilePath& path,
                    const std::string& fingerprint,
                    ComponentPatcher* patcher,
                    ComponentInstaller* installer);

  // If something went wrong during unpacking or installer invocation, the
  // destructor will delete the unpacked CRX files.
  ~ComponentUnpacker();

  Error error() const { return error_; }

  int extended_error() const { return extended_error_; }

 private:
  base::FilePath unpack_path_;
  Error error_;
  int extended_error_;  // Provides additional error information.
};

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_
