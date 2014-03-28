// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"

namespace component_updater {

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
// This class should be used only by the component updater. It is inspired by
// and overlaps with code in the extension's SandboxedUnpacker.
// The main differences are:
// - The public key hash is full SHA256.
// - Does not use a sandboxed unpacker. A valid component is fully trusted.
// - The manifest can have different attributes and resources are not
//   transcoded.
//
// If the CRX is a delta CRX, the flow is:
//   [ComponentUpdater]      [ComponentPatcher]
//   Unpack
//     \_ Verify
//     \_ Unzip
//     \_ BeginPatching ---> DifferentialUpdatePatch
//                             ...
//   EndPatching <------------ ...
//     \_ Install
//     \_ Finish
//
// For a full CRX, the flow is:
//   [ComponentUpdater]
//   Unpack
//     \_ Verify
//     \_ Unzip
//     \_ BeginPatching
//          |
//          V
//   EndPatching
//     \_ Install
//     \_ Finish
//
// In both cases, if there is an error at any point, the remaining steps will
// be skipped and Finish will be called.
class ComponentUnpacker : public base::RefCountedThreadSafe<ComponentUnpacker> {
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

  typedef base::Callback<void(Error, int)> Callback;

  // Constructs an unpacker for a specific component unpacking operation.
  // |pk_hash| is the expected/ public key SHA256 hash. |path| is the current
  // location of the CRX.
  ComponentUnpacker(const std::vector<uint8>& pk_hash,
                    const base::FilePath& path,
                    const std::string& fingerprint,
                    ComponentInstaller* installer,
                    bool in_process,
                    scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Begins the actual unpacking of the files. May invoke a patcher if the
  // package is a differential update. Calls |callback| with the result.
  void Unpack(const Callback& callback);

 private:
  friend class base::RefCountedThreadSafe<ComponentUnpacker>;

  virtual ~ComponentUnpacker();

  bool UnpackInternal();

  // The first step of unpacking is to verify the file. Returns false if an
  // error is encountered, the file is malformed, or the file is incorrectly
  // signed.
  bool Verify();

  // The second step of unpacking is to unzip. Returns false if an error
  // occurs as part of unzipping.
  bool Unzip();

  // The third step is to optionally patch files - this is a no-op for full
  // (non-differential) updates. This step is asynchronous. Returns false if an
  // error is encountered.
  bool BeginPatching();

  // When patching is complete, EndPatching is called before moving on to step
  // four.
  void EndPatching(Error error, int extended_error);

  // The fourth step is to install the unpacked component.
  void Install();

  // The final step is to do clean-up for things that can't be tidied as we go.
  // If there is an error at any step, the remaining steps are skipped and
  // and Finish is called.
  // Finish is responsible for calling the callback provided in Start().
  void Finish();

  std::vector<uint8> pk_hash_;
  base::FilePath path_;
  base::FilePath unpack_path_;
  base::FilePath unpack_diff_path_;
  bool is_delta_;
  std::string fingerprint_;
  scoped_refptr<ComponentPatcher> patcher_;
  ComponentInstaller* installer_;
  Callback callback_;
  const bool in_process_;
  Error error_;
  int extended_error_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ComponentUnpacker);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_UNPACKER_H_
