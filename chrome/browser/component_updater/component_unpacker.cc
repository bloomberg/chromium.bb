// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_unpacker.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/json/json_file_value_serializer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "crypto/secure_hash.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/crx_file.h"
#include "third_party/zlib/google/zip.h"

using crypto::SecureHash;

namespace component_updater {

namespace {

// This class makes sure that the CRX digital signature is valid
// and well formed.
class CRXValidator {
 public:
  explicit CRXValidator(FILE* crx_file) : valid_(false), is_delta_(false) {
    extensions::CrxFile::Header header;
    size_t len = fread(&header, 1, sizeof(header), crx_file);
    if (len < sizeof(header))
      return;

    extensions::CrxFile::Error error;
    scoped_ptr<extensions::CrxFile> crx(
        extensions::CrxFile::Parse(header, &error));
    if (!crx.get())
      return;
    is_delta_ = extensions::CrxFile::HeaderIsDelta(header);

    std::vector<uint8> key(header.key_size);
    len = fread(&key[0], sizeof(uint8), header.key_size, crx_file);
    if (len < header.key_size)
      return;

    std::vector<uint8> signature(header.signature_size);
    len = fread(&signature[0], sizeof(uint8), header.signature_size, crx_file);
    if (len < header.signature_size)
      return;

    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(extension_misc::kSignatureAlgorithm,
                             sizeof(extension_misc::kSignatureAlgorithm),
                             &signature[0],
                             signature.size(),
                             &key[0],
                             key.size())) {
      // Signature verification initialization failed. This is most likely
      // caused by a public key in the wrong format (should encode algorithm).
      return;
    }

    const size_t kBufSize = 8 * 1024;
    scoped_ptr<uint8[]> buf(new uint8[kBufSize]);
    while ((len = fread(buf.get(), 1, kBufSize, crx_file)) > 0)
      verifier.VerifyUpdate(buf.get(), len);

    if (!verifier.VerifyFinal())
      return;

    public_key_.swap(key);
    valid_ = true;
  }

  bool valid() const { return valid_; }

  bool is_delta() const { return is_delta_; }

  const std::vector<uint8>& public_key() const { return public_key_; }

 private:
  bool valid_;
  bool is_delta_;
  std::vector<uint8> public_key_;
};

}  // namespace

ComponentUnpacker::ComponentUnpacker(
    const std::vector<uint8>& pk_hash,
    const base::FilePath& path,
    const std::string& fingerprint,
    ComponentInstaller* installer,
    bool in_process,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : pk_hash_(pk_hash),
      path_(path),
      is_delta_(false),
      fingerprint_(fingerprint),
      installer_(installer),
      in_process_(in_process),
      error_(kNone),
      extended_error_(0),
      task_runner_(task_runner) {
}

// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
scoped_ptr<base::DictionaryValue> ReadManifest(
    const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!base::PathExists(manifest))
    return scoped_ptr<base::DictionaryValue>();
  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  if (!root.get())
    return scoped_ptr<base::DictionaryValue>();
  if (!root->IsType(base::Value::TYPE_DICTIONARY))
    return scoped_ptr<base::DictionaryValue>();
  return scoped_ptr<base::DictionaryValue>(
             static_cast<base::DictionaryValue*>(root.release())).Pass();
}

bool ComponentUnpacker::UnpackInternal() {
  return Verify() && Unzip() && BeginPatching();
}

void ComponentUnpacker::Unpack(const Callback& callback) {
  callback_ = callback;
  if (!UnpackInternal())
    Finish();
}

bool ComponentUnpacker::Verify() {
  VLOG(1) << "Verifying component: " << path_.value();
  if (pk_hash_.empty() || path_.empty()) {
    error_ = kInvalidParams;
    return false;
  }
  // First, validate the CRX header and signature. As of today
  // this is SHA1 with RSA 1024.
  base::ScopedFILE file(base::OpenFile(path_, "rb"));
  if (!file.get()) {
    error_ = kInvalidFile;
    return false;
  }
  CRXValidator validator(file.get());
  file.reset();
  if (!validator.valid()) {
    error_ = kInvalidFile;
    return false;
  }
  is_delta_ = validator.is_delta();

  // File is valid and the digital signature matches. Now make sure
  // the public key hash matches the expected hash. If they do we fully
  // trust this CRX.
  uint8 hash[32] = {};
  scoped_ptr<SecureHash> sha256(SecureHash::Create(SecureHash::SHA256));
  sha256->Update(&(validator.public_key()[0]), validator.public_key().size());
  sha256->Finish(hash, arraysize(hash));

  if (!std::equal(pk_hash_.begin(), pk_hash_.end(), hash)) {
    VLOG(1) << "Hash mismatch: " << path_.value();
    error_ = kInvalidId;
    return false;
  }
  VLOG(1) << "Verification successful: " << path_.value();
  return true;
}

bool ComponentUnpacker::Unzip() {
  base::FilePath& destination = is_delta_ ? unpack_diff_path_ : unpack_path_;
  VLOG(1) << "Unpacking in: " << destination.value();
  if (!base::CreateNewTempDirectory(base::FilePath::StringType(),
                                    &destination)) {
    VLOG(1) << "Unable to create temporary directory for unpacking.";
    error_ = kUnzipPathError;
    return false;
  }
  if (!zip::Unzip(path_, destination)) {
    VLOG(1) << "Unzipping failed.";
    error_ = kUnzipFailed;
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
      error_ = kUnzipPathError;
      return false;
    }
    patcher_ = new ComponentPatcher(unpack_diff_path_,
                                    unpack_path_,
                                    installer_,
                                    in_process_,
                                    task_runner_);
    task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&ComponentPatcher::Start,
                   patcher_,
                   base::Bind(&ComponentUnpacker::EndPatching,
                              scoped_refptr<ComponentUnpacker>(this))));
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&ComponentUnpacker::EndPatching,
                                      scoped_refptr<ComponentUnpacker>(this),
                                      kNone,
                                      0));
  }
  return true;
}

void ComponentUnpacker::EndPatching(Error error, int extended_error) {
  error_ = error;
  extended_error_ = extended_error;
  patcher_ = NULL;
  if (error_ != kNone) {
    Finish();
    return;
  }
  // Optimization: clean up patch files early, in case disk space is too low to
  // install otherwise.
  if (!unpack_diff_path_.empty()) {
    base::DeleteFile(unpack_diff_path_, true);
    unpack_diff_path_.clear();
  }
  Install();
  Finish();
}

void ComponentUnpacker::Install() {
  // Write the fingerprint to disk.
  if (static_cast<int>(fingerprint_.size()) !=
      base::WriteFile(
          unpack_path_.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint_.c_str(),
          fingerprint_.size())) {
    error_ = kFingerprintWriteFailed;
    return;
  }
  scoped_ptr<base::DictionaryValue> manifest(ReadManifest(unpack_path_));
  if (!manifest.get()) {
    error_ = kBadManifest;
    return;
  }
  DCHECK(error_ == kNone);
  if (!installer_->Install(*manifest, unpack_path_)) {
    error_ = kInstallerError;
    return;
  }
}

void ComponentUnpacker::Finish() {
  if (!unpack_diff_path_.empty())
    base::DeleteFile(unpack_diff_path_, true);
  if (!unpack_path_.empty())
    base::DeleteFile(unpack_path_, true);
  callback_.Run(error_, extended_error_);
}

ComponentUnpacker::~ComponentUnpacker() {
}

}  // namespace component_updater
