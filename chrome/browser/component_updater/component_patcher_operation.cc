// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation.h"

#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "courgette/courgette.h"
#include "courgette/third_party/bsdiff.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

using crypto::SecureHash;

namespace component_updater {

namespace {

const char kOutput[] = "output";
const char kSha256[] = "sha256";

// The integer offset disambiguates between overlapping error ranges.
const int kCourgetteErrorOffset = 300;
const int kBsdiffErrorOffset = 600;

}  // namespace

const char kOp[] = "op";
const char kBsdiff[] = "bsdiff";
const char kCourgette[] = "courgette";
const char kInput[] = "input";
const char kPatch[] = "patch";

DeltaUpdateOp* CreateDeltaUpdateOp(
    const std::string& operation,
    scoped_refptr<OutOfProcessPatcher> out_of_process_patcher) {
  if (operation == "copy") {
    return new DeltaUpdateOpCopy();
  } else if (operation == "create") {
    return new DeltaUpdateOpCreate();
  } else if (operation == "bsdiff" || operation == "courgette") {
    return new DeltaUpdateOpPatch(operation, out_of_process_patcher);
  }
  return NULL;
}

DeltaUpdateOp::DeltaUpdateOp() {
}

DeltaUpdateOp::~DeltaUpdateOp() {
}

void DeltaUpdateOp::Run(const base::DictionaryValue* command_args,
                        const base::FilePath& input_dir,
                        const base::FilePath& unpack_dir,
                        ComponentInstaller* installer,
                        const ComponentUnpacker::Callback& callback,
                        scoped_refptr<base::SequencedTaskRunner> task_runner) {
  callback_ = callback;
  task_runner_ = task_runner;
  std::string output_rel_path;
  if (!command_args->GetString(kOutput, &output_rel_path) ||
      !command_args->GetString(kSha256, &output_sha256_)) {
    DoneRunning(ComponentUnpacker::kDeltaBadCommands, 0);
    return;
  }

  output_abs_path_ =
      unpack_dir.Append(base::FilePath::FromUTF8Unsafe(output_rel_path));
  ComponentUnpacker::Error parse_result =
      DoParseArguments(command_args, input_dir, installer);
  if (parse_result != ComponentUnpacker::kNone) {
    DoneRunning(parse_result, 0);
    return;
  }

  const base::FilePath parent = output_abs_path_.DirName();
  if (!base::DirectoryExists(parent)) {
    if (!base::CreateDirectory(parent)) {
      DoneRunning(ComponentUnpacker::kIoError, 0);
      return;
    }
  }

  DoRun(base::Bind(&DeltaUpdateOp::DoneRunning,
                   scoped_refptr<DeltaUpdateOp>(this)));
}

void DeltaUpdateOp::DoneRunning(ComponentUnpacker::Error error,
                                int extended_error) {
  if (error == ComponentUnpacker::kNone)
    error = CheckHash();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(callback_, error, extended_error));
  callback_.Reset();
}

// Uses the hash as a checksum to confirm that the file now residing in the
// output directory probably has the contents it should.
ComponentUnpacker::Error DeltaUpdateOp::CheckHash() {
  std::vector<uint8> expected_hash;
  if (!base::HexStringToBytes(output_sha256_, &expected_hash) ||
      expected_hash.size() != crypto::kSHA256Length)
    return ComponentUnpacker::kDeltaVerificationFailure;

  base::MemoryMappedFile output_file_mmapped;
  if (!output_file_mmapped.Initialize(output_abs_path_))
    return ComponentUnpacker::kDeltaVerificationFailure;

  uint8 actual_hash[crypto::kSHA256Length] = {0};
  const scoped_ptr<SecureHash> hasher(SecureHash::Create(SecureHash::SHA256));
  hasher->Update(output_file_mmapped.data(), output_file_mmapped.length());
  hasher->Finish(actual_hash, sizeof(actual_hash));
  if (memcmp(actual_hash, &expected_hash[0], sizeof(actual_hash)))
    return ComponentUnpacker::kDeltaVerificationFailure;

  return ComponentUnpacker::kNone;
}

scoped_refptr<base::SequencedTaskRunner> DeltaUpdateOp::GetTaskRunner() {
  return task_runner_;
}

DeltaUpdateOpCopy::DeltaUpdateOpCopy() {
}

DeltaUpdateOpCopy::~DeltaUpdateOpCopy() {
}

ComponentUnpacker::Error DeltaUpdateOpCopy::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string input_rel_path;
  if (!command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpCopy::DoRun(const ComponentUnpacker::Callback& callback) {
  if (!base::CopyFile(input_abs_path_, output_abs_path_))
    callback.Run(ComponentUnpacker::kDeltaOperationFailure, 0);
  else
    callback.Run(ComponentUnpacker::kNone, 0);
}

DeltaUpdateOpCreate::DeltaUpdateOpCreate() {
}

DeltaUpdateOpCreate::~DeltaUpdateOpCreate() {
}

ComponentUnpacker::Error DeltaUpdateOpCreate::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpCreate::DoRun(const ComponentUnpacker::Callback& callback) {
  if (!base::Move(patch_abs_path_, output_abs_path_))
    callback.Run(ComponentUnpacker::kDeltaOperationFailure, 0);
  else
    callback.Run(ComponentUnpacker::kNone, 0);
}

DeltaUpdateOpPatch::DeltaUpdateOpPatch(
    const std::string& operation,
    scoped_refptr<OutOfProcessPatcher> out_of_process_patcher)
    : operation_(operation), out_of_process_patcher_(out_of_process_patcher) {
  DCHECK(operation == kBsdiff || operation == kCourgette);
}

DeltaUpdateOpPatch::~DeltaUpdateOpPatch() {
}

ComponentUnpacker::Error DeltaUpdateOpPatch::DoParseArguments(
    const base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  std::string input_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path) ||
      !command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  patch_abs_path_ =
      input_dir.Append(base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

void DeltaUpdateOpPatch::DoRun(const ComponentUnpacker::Callback& callback) {
  if (out_of_process_patcher_.get()) {
    out_of_process_patcher_->Patch(
        operation_,
        GetTaskRunner(),
        input_abs_path_,
        patch_abs_path_,
        output_abs_path_,
        base::Bind(&DeltaUpdateOpPatch::DonePatching, this, callback));
    return;
  }

  if (operation_ == kBsdiff) {
    DonePatching(callback,
                 courgette::ApplyBinaryPatch(
                     input_abs_path_, patch_abs_path_, output_abs_path_));
  } else if (operation_ == kCourgette) {
    DonePatching(
        callback,
        courgette::ApplyEnsemblePatch(input_abs_path_.value().c_str(),
                                      patch_abs_path_.value().c_str(),
                                      output_abs_path_.value().c_str()));
  } else {
    NOTREACHED();
  }
}

void DeltaUpdateOpPatch::DonePatching(
    const ComponentUnpacker::Callback& callback,
    int result) {
  if (operation_ == kBsdiff) {
    if (result == courgette::OK) {
      callback.Run(ComponentUnpacker::kNone, 0);
    } else {
      callback.Run(ComponentUnpacker::kDeltaOperationFailure,
                   result + kBsdiffErrorOffset);
    }
  } else if (operation_ == kCourgette) {
    if (result == courgette::C_OK) {
      callback.Run(ComponentUnpacker::kNone, 0);
    } else {
      callback.Run(ComponentUnpacker::kDeltaOperationFailure,
                   result + kCourgetteErrorOffset);
    }
  } else {
    NOTREACHED();
  }
}

}  // namespace component_updater
