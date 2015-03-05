// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to extract file features for malicious binary detection.
// Each platform has its own implementation of this class.

#ifndef CHROME_BROWSER_SAFE_BROWSING_BINARY_FEATURE_EXTRACTOR_H_
#define CHROME_BROWSER_SAFE_BROWSING_BINARY_FEATURE_EXTRACTOR_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}

namespace safe_browsing {
class ClientDownloadRequest_Digests;
class ClientDownloadRequest_ImageHeaders;
class ClientDownloadRequest_SignatureInfo;

class BinaryFeatureExtractor
    : public base::RefCountedThreadSafe<BinaryFeatureExtractor> {
 public:
  // The type and defined values for a bitfield that controls aspects of image
  // header extraction.
  typedef uint32_t ExtractHeadersOption;
  static const ExtractHeadersOption kDefaultOptions = 0;
  static const ExtractHeadersOption kOmitExports = 1U << 0;

  BinaryFeatureExtractor();

  // Fills in the DownloadRequest_SignatureInfo for the given file path.
  // This method may be called on any thread.
  virtual void CheckSignature(
      const base::FilePath& file_path,
      ClientDownloadRequest_SignatureInfo* signature_info);

  // Populates |image_headers| with the PE image headers of |file_path|.
  // |options| is a bitfield controlling aspects of extraction. Returns true if
  // |image_headers| is populated with any information.
  virtual bool ExtractImageHeaders(
      const base::FilePath& file_path,
      ExtractHeadersOption options,
      ClientDownloadRequest_ImageHeaders* image_headers);

  // Populates |digests.sha256| with the SHA256 digest of |file_path|.
  virtual void ExtractDigest(const base::FilePath& file_path,
                             ClientDownloadRequest_Digests* digests);

 protected:
  friend class base::RefCountedThreadSafe<BinaryFeatureExtractor>;
  virtual ~BinaryFeatureExtractor();

 private:
  DISALLOW_COPY_AND_ASSIGN(BinaryFeatureExtractor);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_BINARY_FEATURE_EXTRACTOR_H_
