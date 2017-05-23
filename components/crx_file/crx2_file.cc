// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crx_file/crx2_file.h"

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/crx_file/id_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"

namespace crx_file {

namespace {

// The current version of the crx format.
static const uint32_t kCurrentVersion = 2;

// The current version of the crx diff format.
static const uint32_t kCurrentDiffVersion = 0;

// The maximum size the crx parser will tolerate for a public key.
static const uint32_t kMaxPublicKeySize = 1 << 16;

// The maximum size the crx parser will tolerate for a signature.
static const uint32_t kMaxSignatureSize = 1 << 16;

}  // namespace

std::unique_ptr<Crx2File> Crx2File::Create(const uint32_t key_size,
                                           const uint32_t signature_size,
                                           Crx2File::Error* error) {
  Crx2File::Header header;
  memcpy(&header.magic, kCrx2FileHeaderMagic, kCrx2FileHeaderMagicSize);
  header.version = kCurrentVersion;
  header.key_size = key_size;
  header.signature_size = signature_size;
  if (HeaderIsValid(header, error))
    return base::WrapUnique(new Crx2File(header));
  return nullptr;
}

Crx2File::Crx2File(const Header& header) : header_(header) {}

bool Crx2File::HeaderIsValid(const Crx2File::Header& header,
                             Crx2File::Error* error) {
  bool valid = false;
  bool diffCrx = false;
  if (!strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic)))
    diffCrx = true;
  if (strncmp(kCrx2FileHeaderMagic, header.magic, sizeof(header.magic)) &&
      !diffCrx)
    *error = kWrongMagic;
  else if (header.version != kCurrentVersion &&
           !(diffCrx && header.version == kCurrentDiffVersion))
    *error = kInvalidVersion;
  else if (header.key_size > kMaxPublicKeySize)
    *error = kInvalidKeyTooLarge;
  else if (header.key_size == 0)
    *error = kInvalidKeyTooSmall;
  else if (header.signature_size > kMaxSignatureSize)
    *error = kInvalidSignatureTooLarge;
  else if (header.signature_size == 0)
    *error = kInvalidSignatureTooSmall;
  else
    valid = true;
  return valid;
}

}  // namespace crx_file
