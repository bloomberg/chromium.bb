// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/component_unpacker.h"

#include <stdint.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component_patcher.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "third_party/zlib/google/zip.h"

namespace update_client {

// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
std::unique_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest))
    return std::unique_ptr<base::DictionaryValue>();
  JSONFileValueDeserializer deserializer(manifest);
  std::string error;
  std::unique_ptr<base::Value> root = deserializer.Deserialize(NULL, &error);
  if (!root.get())
    return std::unique_ptr<base::DictionaryValue>();
  if (!root->IsType(base::Value::Type::DICTIONARY))
    return std::unique_ptr<base::DictionaryValue>();
  return std::unique_ptr<base::DictionaryValue>(
      static_cast<base::DictionaryValue*>(root.release()));
}

ComponentUnpacker::Result::Result() {}

ComponentUnpacker::ComponentUnpacker(
    const std::vector<uint8_t>& pk_hash,
    const base::FilePath& path,
    const scoped_refptr<CrxInstaller>& installer,
    const scoped_refptr<OutOfProcessPatcher>& oop_patcher,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : pk_hash_(pk_hash),
      path_(path),
      is_delta_(false),
      installer_(installer),
      oop_patcher_(oop_patcher),
      error_(UnpackerError::kNone),
      extended_error_(0),
      task_runner_(task_runner) {}

ComponentUnpacker::~ComponentUnpacker() {}

bool ComponentUnpacker::UnpackInternal() {
  return Verify() && Unzip() && BeginPatching();
}

void ComponentUnpacker::Unpack(const Callback& callback) {
  callback_ = callback;
  if (!UnpackInternal())
    EndUnpacking();
}

bool ComponentUnpacker::Verify() {
  VLOG(1) << "Verifying component: " << path_.value();
  if (pk_hash_.empty() || path_.empty()) {
    error_ = UnpackerError::kInvalidParams;
    return false;
  }
  const std::vector<std::vector<uint8_t>> required_keys = {pk_hash_};
  const crx_file::VerifierResult result =
      crx_file::Verify(path_, crx_file::VerifierFormat::CRX2_OR_CRX3,
                       required_keys, std::vector<uint8_t>(), nullptr, nullptr);
  if (result != crx_file::VerifierResult::OK_FULL &&
      result != crx_file::VerifierResult::OK_DELTA) {
    error_ = UnpackerError::kInvalidFile;
    extended_error_ = static_cast<int>(result);
    return false;
  }
  is_delta_ = result == crx_file::VerifierResult::OK_DELTA;
  VLOG(1) << "Verification successful: " << path_.value();
  return true;
}

bool ComponentUnpacker::Unzip() {
  // Mind the reference to non-const type, passed as an argument below.
  base::FilePath& destination = is_delta_ ? unpack_diff_path_ : unpack_path_;
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &destination)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    error_ = UnpackerError::kUnzipPathError;
    return false;
  }
  VLOG(1) << "Unpacking in: " << destination.value();
  if (!zip::Unzip(path_, destination)) {
    VLOG(1) << "Unzipping failed.";
    error_ = UnpackerError::kUnzipFailed;
    return false;
  }
  VLOG(1) << "Unpacked successfully";
  return true;
}

bool ComponentUnpacker::BeginPatching() {
  if (is_delta_) {  // Package is a diff package.
    // Use a different temp directory for the patch output files.
    if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                      &unpack_path_)) {
      error_ = UnpackerError::kUnzipPathError;
      return false;
    }
    patcher_ = new ComponentPatcher(unpack_diff_path_, unpack_path_, installer_,
                                    oop_patcher_, task_runner_);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ComponentPatcher::Start, patcher_,
                   base::Bind(&ComponentUnpacker::EndPatching,
                              scoped_refptr<ComponentUnpacker>(this))));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&ComponentUnpacker::EndPatching,
                                      scoped_refptr<ComponentUnpacker>(this),
                                      UnpackerError::kNone, 0));
  }
  return true;
}

void ComponentUnpacker::EndPatching(UnpackerError error, int extended_error) {
  error_ = error;
  extended_error_ = extended_error;
  patcher_ = NULL;

  EndUnpacking();
}

void ComponentUnpacker::EndUnpacking() {
  if (!unpack_diff_path_.empty())
    base::DeleteFile(unpack_diff_path_, true);
  if (error_ != UnpackerError::kNone && !unpack_path_.empty())
    base::DeleteFile(unpack_path_, true);

  Result result;
  result.error = error_;
  result.extended_error = extended_error_;
  if (error_ == UnpackerError::kNone)
    result.unpack_path = unpack_path_;

  task_runner_->PostTask(FROM_HERE, base::Bind(callback_, result));
}

}  // namespace update_client
