// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_unpacker.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/memory/scoped_handle.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/sandboxed_extension_unpacker.h"
#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/zip.h"
#include "content/common/json_value_serializer.h"
#include "crypto/secure_hash.h"
#include "crypto/signature_verifier.h"

using crypto::SecureHash;

namespace {
// This class makes sure that the CRX digital signature is valid
// and well formed.
class CRXValidator {
 public:
  enum Result {
    kValid,
    kInvalidHeader,
    kWrongMagic,
    kInvalidVersion,
    kInvalidKey,
    kInvalidSignature,
    kWrongKeyFormat,
    kSignatureMismatch,
    kLast
  };

  explicit CRXValidator(FILE* crx_file) : result_(kLast) {
    SXU::ExtensionHeader header;
    size_t len = fread(&header, 1, sizeof(header), crx_file);
    if (len < sizeof(header)) {
      result_ = kInvalidHeader;
      return;
    }
    if (strncmp(SXU::kExtensionHeaderMagic, header.magic,
        sizeof(header.magic))) {
      result_ = kWrongMagic;
      return;
    }
    if (header.version != SXU::kCurrentVersion) {
      result_ = kInvalidVersion;
      return;
    }
    if ((header.key_size > SXU::kMaxPublicKeySize) ||
        (header.key_size == 0)) {
      result_ = kInvalidKey;
      return;
    }
    if ((header.signature_size > SXU::kMaxSignatureSize) ||
        (header.signature_size == 0)) {
      result_ = kInvalidSignature;
      return;
    }

    std::vector<uint8> key(header.key_size);
    len = fread(&key[0], sizeof(uint8), header.key_size, crx_file);
    if (len < header.key_size) {
      result_ = kInvalidKey;
      return;
    }

    std::vector<uint8> signature(header.signature_size);
    len = fread(&signature[0], sizeof(uint8), header.signature_size, crx_file);
    if (len < header.signature_size) {
      result_ = kInvalidSignature;
      return;
    }

    crypto::SignatureVerifier verifier;
    if (!verifier.VerifyInit(extension_misc::kSignatureAlgorithm,
                             sizeof(extension_misc::kSignatureAlgorithm),
                             &signature[0], signature.size(),
                             &key[0], key.size())) {
      // Signature verification initialization failed. This is most likely
      // caused by a public key in the wrong format (should encode algorithm).
      result_ = kWrongKeyFormat;
      return;
    }

    const size_t kBufSize = 8 * 1024;
    scoped_array<uint8> buf(new uint8[kBufSize]);
    while ((len = fread(buf.get(), 1, kBufSize, crx_file)) > 0)
      verifier.VerifyUpdate(buf.get(), len);

    if (!verifier.VerifyFinal()) {
      result_ = kSignatureMismatch;
      return;
    }

    public_key_.swap(key);
    result_ = kValid;
  }

  Result result() const { return result_; }

  const std::vector<uint8>& public_key() const { return public_key_; }

 private:
  // TODO(cpu): Move the kExtensionHeaderMagic constants to a better header
  // right now it feels we are reaching too deep into the extension code.
  typedef SandboxedExtensionUnpacker SXU;
  Result result_;
  std::vector<uint8> public_key_;
};

// Deserialize the CRX manifest. The top level must be a dictionary.
// TODO(cpu): add a specific attribute check to a component json that the
// extension unpacker will reject, so that a component cannot be installed
// as an extension.
base::DictionaryValue* ReadManifest(const FilePath& unpack_path) {
  FilePath manifest = unpack_path.Append(FILE_PATH_LITERAL("manifest.json"));
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

}  // namespace.

ComponentUnpacker::ComponentUnpacker(const std::vector<uint8>& pk_hash,
                                     const FilePath& path,
                                     ComponentInstaller* installer)
  : error_(kNone) {
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
  if (validator.result() != CRXValidator::kValid) {
    error_ = kInvalidFile;
    return;
  }
  file.Close();

  // File is vaild and the digital signature matches. Now make sure
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
  // we can easily find the package in a end user machine.
  std::string dir(StringPrintf("CRX_%s", base::HexEncode(hash, 6).c_str()));
  unpack_path_ = path.DirName().AppendASCII(dir.c_str());
  if (file_util::DirectoryExists(unpack_path_)) {
    if (!file_util::Delete(unpack_path_, true)) {
      unpack_path_.clear();
      error_ = kUzipPathError;
      return;
    }
  }
  if (!file_util::CreateDirectory(unpack_path_)) {
    unpack_path_.clear();
    error_ = kUzipPathError;
    return;
  }
  if (!Unzip(path, unpack_path_)) {
    error_ = kUnzipFailed;
    return;
  }
  scoped_ptr<base::DictionaryValue> manifest(ReadManifest(unpack_path_));
  if (!manifest.get()) {
    error_ = kBadManifest;
    return;
  }
  if (!installer->Install(manifest.release(), unpack_path_)) {
    error_ = kInstallerError;
    return;
  }
  // Installation succesful. The directory is not our concern now.
  unpack_path_.clear();
}

ComponentUnpacker::~ComponentUnpacker() {
  if (!unpack_path_.empty()) {
    file_util::Delete(unpack_path_, true);
  }
}
