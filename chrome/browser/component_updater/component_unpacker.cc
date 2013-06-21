// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_unpacker.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/component_updater/component_patcher.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "crypto/secure_hash.h"
#include "crypto/signature_verifier.h"
#include "extensions/common/crx_file.h"
#include "third_party/zlib/google/zip.h"

using crypto::SecureHash;

namespace {

// This class makes sure that the CRX digital signature is valid
// and well formed.
class CRXValidator {
 public:
  explicit CRXValidator(FILE* crx_file) : valid_(false), delta_(false) {
    extensions::CrxFile::Header header;
    size_t len = fread(&header, 1, sizeof(header), crx_file);
    if (len < sizeof(header))
      return;

    extensions::CrxFile::Error error;
    scoped_ptr<extensions::CrxFile> crx(
        extensions::CrxFile::Parse(header, &error));
    if (!crx.get())
      return;
    delta_ = extensions::CrxFile::HeaderIsDelta(header);

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
                             &signature[0], signature.size(),
                             &key[0], key.size())) {
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

  bool delta() const { return delta_; }

  const std::vector<uint8>& public_key() const { return public_key_; }

 private:
  bool valid_;
  bool delta_;
  std::vector<uint8> public_key_;
};

// Deserialize the CRX manifest. The top level must be a dictionary.
// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
base::DictionaryValue* ReadManifest(const base::FilePath& unpack_path) {
  base::FilePath manifest =
      unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
  if (!file_util::PathExists(manifest))
    return NULL;
  JSONFileValueSerializer serializer(manifest);
  std::string error;
  scoped_ptr<base::Value> root(serializer.Deserialize(NULL, &error));
  if (!root.get())
    return NULL;
  if (!root->IsType(base::Value::TYPE_DICTIONARY))
    return NULL;
  return static_cast<base::DictionaryValue*>(root.release());
}

// Deletes a path if it exists, and then creates a directory there.
// Returns true if and only if these operations were successful.
// This method doesn't take any special steps to prevent files from
// being inserted into the target directory by another process or thread.
bool MakeEmptyDirectory(const base::FilePath& path) {
  if (file_util::PathExists(path)) {
    if (!file_util::Delete(path, true))
      return false;
  }
  if (!file_util::CreateDirectory(path))
    return false;
  return true;
}

}  // namespace.

ComponentUnpacker::ComponentUnpacker(const std::vector<uint8>& pk_hash,
                                     const base::FilePath& path,
                                     const std::string& fingerprint,
                                     ComponentPatcher* patcher,
                                     ComponentInstaller* installer)
    : error_(kNone),
      extended_error_(0) {
  if (pk_hash.empty() || path.empty()) {
    error_ = kInvalidParams;
    return;
  }
  // First, validate the CRX header and signature. As of today
  // this is SHA1 with RSA 1024.
  ScopedStdioHandle file(file_util::OpenFile(path, "rb"));
  if (!file.get()) {
    error_ = kInvalidFile;
    return;
  }
  CRXValidator validator(file.get());
  if (!validator.valid()) {
    error_ = kInvalidFile;
    return;
  }
  file.Close();

  // File is valid and the digital signature matches. Now make sure
  // the public key hash matches the expected hash. If they do we fully
  // trust this CRX.
  uint8 hash[32];
  scoped_ptr<SecureHash> sha256(SecureHash::Create(SecureHash::SHA256));
  sha256->Update(&(validator.public_key()[0]), validator.public_key().size());
  sha256->Finish(hash, arraysize(hash));

  if (!std::equal(pk_hash.begin(), pk_hash.end(), hash)) {
    error_ = kInvalidId;
    return;
  }
  // We want the temporary directory to be unique and yet predictable, so
  // we can easily find the package in an end user machine.
  const std::string dir(
      base::StringPrintf("CRX_%s", base::HexEncode(hash, 6).c_str()));
  unpack_path_ = path.DirName().AppendASCII(dir.c_str());
  if (!MakeEmptyDirectory(unpack_path_)) {
    unpack_path_.clear();
    error_ = kUnzipPathError;
    return;
  }
  if (validator.delta()) {  // Package is a diff package.
    // We want a different temp directory for the delta files; we'll put the
    // patch output into unpack_path_.
    std::string dir(
        base::StringPrintf("CRX_%s_diff", base::HexEncode(hash, 6).c_str()));
    base::FilePath unpack_diff_path = path.DirName().AppendASCII(dir.c_str());
    if (!MakeEmptyDirectory(unpack_diff_path)) {
      error_ = kUnzipPathError;
      return;
    }
    if (!zip::Unzip(path, unpack_diff_path)) {
      error_ = kUnzipFailed;
      return;
    }
    ComponentUnpacker::Error result = DifferentialUpdatePatch(unpack_diff_path,
                                                              unpack_path_,
                                                              patcher,
                                                              installer,
                                                              &extended_error_);
    file_util::Delete(unpack_diff_path, true);
    unpack_diff_path.clear();
    error_ = result;
    if (error_ != kNone) {
      return;
    }
  } else {
    // Package is a normal update/install; unzip it into unpack_path_ directly.
    if (!zip::Unzip(path, unpack_path_)) {
      error_ = kUnzipFailed;
      return;
    }
  }
  scoped_ptr<base::DictionaryValue> manifest(ReadManifest(unpack_path_));
  if (!manifest.get()) {
    error_ = kBadManifest;
    return;
  }
  // Write the fingerprint to disk.
  if (static_cast<int>(fingerprint.size()) !=
      file_util::WriteFile(
          unpack_path_.Append(FILE_PATH_LITERAL("manifest.fingerprint")),
          fingerprint.c_str(),
          fingerprint.size())) {
    error_ = kFingerprintWriteFailed;
    return;
  }
  if (!installer->Install(*manifest, unpack_path_)) {
    error_ = kInstallerError;
    return;
  }
  // Installation successful. The directory is not our concern now.
  unpack_path_.clear();
}

ComponentUnpacker::~ComponentUnpacker() {
  if (!unpack_path_.empty())
    file_util::Delete(unpack_path_, true);
}
