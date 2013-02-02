// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utility functions to check executable signatures for malicious binary
// detection.  Each platform has its own implementation of this class.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_
#define CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
}

namespace safe_browsing {
class ClientDownloadRequest_SignatureInfo;

class SignatureUtil : public base::RefCountedThreadSafe<SignatureUtil> {
 public:
  SignatureUtil();

  // Fills in the DownloadRequest_SignatureInfo for the given file path.
  // This method may be called on any thread.
  virtual void CheckSignature(
      const base::FilePath& file_path,
      ClientDownloadRequest_SignatureInfo* signature_info);

 protected:
  friend class base::RefCountedThreadSafe<SignatureUtil>;
  virtual ~SignatureUtil();

 private:
  DISALLOW_COPY_AND_ASSIGN(SignatureUtil);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SIGNATURE_UTIL_H_
