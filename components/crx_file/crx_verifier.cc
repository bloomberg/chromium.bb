// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx_verifier.h"

#include <cstring>
#include <memory>

#include "base/base64.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/crx_file/crx2_file.h"
#include "components/crx_file/id_util.h"
#include "crypto/secure_hash.h"
#include "crypto/secure_util.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

namespace crx_file {

namespace {

// The maximum size the crx2 parser will tolerate for a public key.
constexpr uint32_t kMaxPublicKeySize = 1 << 16;

// The maximum size the crx2 parser will tolerate for a signature.
constexpr uint32_t kMaxSignatureSize = 1 << 16;

int ReadAndHashBuffer(uint8_t* buffer,
                      int length,
                      base::File* file,
                      crypto::SecureHash* hash) {
  static_assert(sizeof(char) == sizeof(uint8_t), "Unsupported char size.");
  int read = file->ReadAtCurrentPos(reinterpret_cast<char*>(buffer), length);
  hash->Update(buffer, read);
  return read;
}

// Returns UINT32_MAX in the case of an unexpected EOF or read error, else
// returns the read uint32.
uint32_t ReadAndHashLittleEndianUInt32(base::File* file,
                                       crypto::SecureHash* hash) {
  uint8_t buffer[4] = {};
  if (ReadAndHashBuffer(buffer, 4, file, hash) != 4)
    return UINT32_MAX;
  return buffer[3] << 24 | buffer[2] << 16 | buffer[1] << 8 | buffer[0];
}

VerifierResult VerifyCrx3(
    base::File* file,
    crypto::SecureHash* hash,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    std::string* public_key,
    std::string* crx_id) {
  // Crx3 files are not yet supported - treat this as a malformed header.
  return VerifierResult::ERROR_HEADER_INVALID;
}

VerifierResult VerifyCrx2(
    base::File* file,
    crypto::SecureHash* hash,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    std::string* public_key,
    std::string* crx_id) {
  const uint32_t key_size = ReadAndHashLittleEndianUInt32(file, hash);
  if (key_size > kMaxPublicKeySize)
    return VerifierResult::ERROR_HEADER_INVALID;
  const uint32_t sig_size = ReadAndHashLittleEndianUInt32(file, hash);
  if (sig_size > kMaxSignatureSize)
    return VerifierResult::ERROR_HEADER_INVALID;
  std::vector<uint8_t> key(key_size);
  if (ReadAndHashBuffer(key.data(), key_size, file, hash) !=
      static_cast<int>(key_size))
    return VerifierResult::ERROR_HEADER_INVALID;
  for (const auto& expected_hash : required_key_hashes) {
    // In practice we expect zero or one key_hashes_ for CRX2 files.
    std::vector<uint8_t> hash(crypto::kSHA256Length);
    std::unique_ptr<crypto::SecureHash> sha256 =
        crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    sha256->Update(key.data(), key.size());
    sha256->Finish(hash.data(), hash.size());
    if (hash != expected_hash)
      return VerifierResult::ERROR_REQUIRED_PROOF_MISSING;
  }

  std::vector<uint8_t> sig(sig_size);
  if (ReadAndHashBuffer(sig.data(), sig_size, file, hash) !=
      static_cast<int>(sig_size))
    return VerifierResult::ERROR_HEADER_INVALID;
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(crypto::SignatureVerifier::RSA_PKCS1_SHA1,
                           sig.data(), sig.size(), key.data(), key.size())) {
    return VerifierResult::ERROR_SIGNATURE_INITIALIZATION_FAILED;
  }

  // Read the rest of the file.
  uint8_t buffer[1 << 12] = {};
  size_t len = 0;
  while ((len = ReadAndHashBuffer(buffer, arraysize(buffer), file, hash)) > 0)
    verifier.VerifyUpdate(buffer, len);
  if (!verifier.VerifyFinal())
    return VerifierResult::ERROR_SIGNATURE_VERIFICATION_FAILED;

  const std::string public_key_bytes(key.begin(), key.end());
  if (public_key)
    base::Base64Encode(public_key_bytes, public_key);
  if (crx_id)
    *crx_id = id_util::GenerateId(public_key_bytes);
  return VerifierResult::OK_FULL;
}

}  // namespace

VerifierResult Verify(
    const base::FilePath& crx_path,
    const VerifierFormat& format,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    const std::vector<uint8_t>& required_file_hash,
    std::string* public_key,
    std::string* crx_id) {
  base::File file(crx_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return VerifierResult::ERROR_FILE_NOT_READABLE;

  std::unique_ptr<crypto::SecureHash> file_hash =
      crypto::SecureHash::Create(crypto::SecureHash::SHA256);

  // Magic number.
  bool diff = false;
  char buffer[kCrx2FileHeaderMagicSize] = {};
  if (file.ReadAtCurrentPos(buffer, kCrx2FileHeaderMagicSize) !=
      kCrx2FileHeaderMagicSize)
    return VerifierResult::ERROR_HEADER_INVALID;
  if (!strncmp(buffer, kCrxDiffFileHeaderMagic, kCrx2FileHeaderMagicSize))
    diff = true;
  else if (strncmp(buffer, kCrx2FileHeaderMagic, kCrx2FileHeaderMagicSize))
    return VerifierResult::ERROR_HEADER_INVALID;
  file_hash->Update(buffer, sizeof(buffer));

  // Version number.
  const uint32_t version =
      ReadAndHashLittleEndianUInt32(&file, file_hash.get());
  VerifierResult result;
  if (format == VerifierFormat::CRX2_OR_CRX3 &&
      (version == 2 || (diff && version == 0)))
    result = VerifyCrx2(&file, file_hash.get(), required_key_hashes, public_key,
                        crx_id);
  else if (version == 3)
    result = VerifyCrx3(&file, file_hash.get(), required_key_hashes, public_key,
                        crx_id);
  else
    result = VerifierResult::ERROR_HEADER_INVALID;
  if (result != VerifierResult::OK_FULL)
    return result;

  // Finalize file hash.
  uint8_t final_hash[crypto::kSHA256Length] = {};
  file_hash->Finish(final_hash, sizeof(final_hash));
  if (!required_file_hash.empty()) {
    if (required_file_hash.size() != crypto::kSHA256Length)
      return VerifierResult::ERROR_EXPECTED_HASH_INVALID;
    if (!crypto::SecureMemEqual(final_hash, required_file_hash.data(),
                                crypto::kSHA256Length))
      return VerifierResult::ERROR_FILE_HASH_FAILED;
  }
  return diff ? VerifierResult::OK_DELTA : VerifierResult::OK_FULL;
}

}  // namespace crx_file
