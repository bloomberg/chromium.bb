// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_patcher_operation.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_handle.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/crx_file.h"
#include "third_party/zlib/google/zip.h"

using crypto::SecureHash;

namespace component_updater {

namespace {

const char kInput[] = "input";
const char kOp[] = "op";
const char kOutput[] = "output";
const char kPatch[] = "patch";
const char kSha256[] = "sha256";

}  // namespace

DeltaUpdateOp* CreateDeltaUpdateOp(base::DictionaryValue* command) {
  std::string operation;
  if (!command->GetString(kOp, &operation))
    return NULL;
  if (operation == "copy")
    return new DeltaUpdateOpCopy();
  else if (operation == "create")
    return new DeltaUpdateOpCreate();
  else if (operation == "bsdiff")
    return new DeltaUpdateOpPatchBsdiff();
  else if (operation == "courgette")
    return new DeltaUpdateOpPatchCourgette();
  return NULL;
}

DeltaUpdateOp::DeltaUpdateOp() {}

DeltaUpdateOp::~DeltaUpdateOp() {}

ComponentUnpacker::Error DeltaUpdateOp::Run(base::DictionaryValue* command_args,
                                            const base::FilePath& input_dir,
                                            const base::FilePath& unpack_dir,
                                            ComponentPatcher* patcher,
                                            ComponentInstaller* installer,
                                            int* error) {
  std::string output_rel_path;
  if (!command_args->GetString(kOutput, &output_rel_path) ||
      !command_args->GetString(kSha256, &output_sha256_))
    return ComponentUnpacker::kDeltaBadCommands;

  output_abs_path_ = unpack_dir.Append(
      base::FilePath::FromUTF8Unsafe(output_rel_path));
  ComponentUnpacker::Error parse_result = DoParseArguments(
      command_args, input_dir, installer);
  if (parse_result != ComponentUnpacker::kNone)
    return parse_result;

  const base::FilePath parent = output_abs_path_.DirName();
  if (!base::DirectoryExists(parent)) {
    if (!base::CreateDirectory(parent))
      return ComponentUnpacker::kIoError;
  }

  ComponentUnpacker::Error run_result = DoRun(patcher, error);
  if (run_result != ComponentUnpacker::kNone)
    return run_result;

  return CheckHash();
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

DeltaUpdateOpCopy::DeltaUpdateOpCopy() {}

ComponentUnpacker::Error DeltaUpdateOpCopy::DoParseArguments(
    base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string input_rel_path;
  if (!command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  return ComponentUnpacker::kNone;
}

ComponentUnpacker::Error DeltaUpdateOpCopy::DoRun(ComponentPatcher*,
                                                  int* error) {
  *error = 0;
  if (!base::CopyFile(input_abs_path_, output_abs_path_))
    return ComponentUnpacker::kDeltaOperationFailure;

  return ComponentUnpacker::kNone;
}

DeltaUpdateOpCreate::DeltaUpdateOpCreate() {}

ComponentUnpacker::Error DeltaUpdateOpCreate::DoParseArguments(
    base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  patch_abs_path_ = input_dir.Append(
      base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

ComponentUnpacker::Error DeltaUpdateOpCreate::DoRun(ComponentPatcher*,
                                                    int* error) {
  *error = 0;
  if (!base::Move(patch_abs_path_, output_abs_path_))
    return ComponentUnpacker::kDeltaOperationFailure;

  return ComponentUnpacker::kNone;
}

DeltaUpdateOpPatchBsdiff::DeltaUpdateOpPatchBsdiff() {}

ComponentUnpacker::Error DeltaUpdateOpPatchBsdiff::DoParseArguments(
    base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  std::string input_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path) ||
      !command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  patch_abs_path_ = input_dir.Append(
      base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

ComponentUnpacker::Error DeltaUpdateOpPatchBsdiff::DoRun(
    ComponentPatcher* patcher,
    int* error) {
  *error = 0;
  return patcher->Patch(ComponentPatcher::kPatchTypeBsdiff,
                        input_abs_path_,
                        patch_abs_path_,
                        output_abs_path_,
                        error);
}

DeltaUpdateOpPatchCourgette::DeltaUpdateOpPatchCourgette() {}

ComponentUnpacker::Error DeltaUpdateOpPatchCourgette::DoParseArguments(
    base::DictionaryValue* command_args,
    const base::FilePath& input_dir,
    ComponentInstaller* installer) {
  std::string patch_rel_path;
  std::string input_rel_path;
  if (!command_args->GetString(kPatch, &patch_rel_path) ||
      !command_args->GetString(kInput, &input_rel_path))
    return ComponentUnpacker::kDeltaBadCommands;

  if (!installer->GetInstalledFile(input_rel_path, &input_abs_path_))
    return ComponentUnpacker::kDeltaMissingExistingFile;

  patch_abs_path_ = input_dir.Append(
      base::FilePath::FromUTF8Unsafe(patch_rel_path));

  return ComponentUnpacker::kNone;
}

ComponentUnpacker::Error DeltaUpdateOpPatchCourgette::DoRun(
    ComponentPatcher* patcher,
    int* error) {
  *error = 0;
  return patcher->Patch(ComponentPatcher::kPatchTypeCourgette,
                        input_abs_path_,
                        patch_abs_path_,
                        output_abs_path_,
                        error);
}

}  // namespace component_updater
