// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/crx_file.h"

namespace extensions {

namespace {

// The current version of the crx format.
static const uint32 kCurrentVersion = 2;

// The current version of the crx diff format.
static const uint32 kCurrentDiffVersion = 0;

// The maximum size the crx parser will tolerate for a public key.
static const uint32 kMaxPublicKeySize = 1 << 16;

// The maximum size the crx parser will tolerate for a signature.
static const uint32 kMaxSignatureSize = 1 << 16;

}  // namespace

// The magic string embedded in the header.
const char kCrxFileHeaderMagic[] = "Cr24";
const char kCrxDiffFileHeaderMagic[] = "CrOD";

scoped_ptr<CrxFile> CrxFile::Parse(const CrxFile::Header& header,
                                   CrxFile::Error* error) {
  if (HeaderIsValid(header, error))
    return scoped_ptr<CrxFile>(new CrxFile(header));
  return scoped_ptr<CrxFile>();
}

scoped_ptr<CrxFile> CrxFile::Create(const uint32 key_size,
                                    const uint32 signature_size,
                                    CrxFile::Error* error) {
  CrxFile::Header header;
  memcpy(&header.magic, kCrxFileHeaderMagic, kCrxFileHeaderMagicSize);
  header.version = kCurrentVersion;
  header.key_size = key_size;
  header.signature_size = signature_size;
  if (HeaderIsValid(header, error))
    return scoped_ptr<CrxFile>(new CrxFile(header));
  return scoped_ptr<CrxFile>();
}

CrxFile::CrxFile(const Header& header) : header_(header) {
}

bool CrxFile::HeaderIsDelta(const CrxFile::Header& header) {
  return !strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic));
}

bool CrxFile::HeaderIsValid(const CrxFile::Header& header,
                            CrxFile::Error* error) {
  bool valid = false;
  bool diffCrx = false;
  if (!strncmp(kCrxDiffFileHeaderMagic, header.magic, sizeof(header.magic)))
    diffCrx = true;
  if (strncmp(kCrxFileHeaderMagic, header.magic, sizeof(header.magic)) &&
      !diffCrx)
    *error = kWrongMagic;
  else if (header.version != kCurrentVersion
      && !(diffCrx && header.version == kCurrentDiffVersion))
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

}  // namespace extensions
