// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRX_FILE_CRX2_FILE_H_
#define COMPONENTS_CRX_FILE_CRX2_FILE_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>
#include <vector>

namespace crx_file {

// The magic string embedded in the header.
constexpr char kCrx2FileHeaderMagic[] = "Cr24";
constexpr char kCrxDiffFileHeaderMagic[] = "CrOD";
constexpr int kCrx2FileHeaderMagicSize = 4;

// CRX files have a header that includes a magic key, version number, and
// some signature sizing information. Use Crx2File object to validate whether
// the header is valid or not.
class Crx2File {
 public:
  // This header is the first data at the beginning of an extension. Its
  // contents are purposely 32-bit aligned so that it can just be slurped into
  // a struct without manual parsing.
  struct Header {
    char magic[kCrx2FileHeaderMagicSize];
    uint32_t version;
    uint32_t key_size;        // The size of the public key, in bytes.
    uint32_t signature_size;  // The size of the signature, in bytes.
    // An ASN.1-encoded PublicKeyInfo structure follows.
    // The signature follows.
  };

  enum Error {
    kWrongMagic,
    kInvalidVersion,
    kInvalidKeyTooLarge,
    kInvalidKeyTooSmall,
    kInvalidSignatureTooLarge,
    kInvalidSignatureTooSmall,
    kMaxValue,
  };

  // Construct a new header for the given key and signature sizes.
  // Returns null if erroneous values of |key_size| and/or
  // |signature_size| are provided. |error| contains an error code with
  // additional information.
  // Use this constructor and then .header() to obtain the Header
  // for writing out to a CRX file.
  static std::unique_ptr<Crx2File> Create(const uint32_t key_size,
                                          const uint32_t signature_size,
                                          Error* error);

  // Returns the header structure for writing out to a CRX file.
  const Header& header() const { return header_; }

 private:
  Header header_;

  // Constructor is private. Clients should use static factory methods above.
  explicit Crx2File(const Header& header);

  // Checks the |header| for validity and returns true if the values are valid.
  // If false is returned, more detailed error code is returned in |error|.
  static bool HeaderIsValid(const Header& header, Error* error);
};

}  // namespace crx_file

#endif  // COMPONENTS_CRX_FILE_CRX2_FILE_H_
