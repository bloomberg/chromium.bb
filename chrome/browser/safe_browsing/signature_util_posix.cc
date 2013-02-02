// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a stub for the code signing utilities on Mac and Linux.
// It should eventually be replaced with a real implementation.

#include "chrome/browser/safe_browsing/signature_util.h"

namespace safe_browsing {

SignatureUtil::SignatureUtil() {}

SignatureUtil::~SignatureUtil() {}

void SignatureUtil::CheckSignature(
    const base::FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {}

}  // namespace safe_browsing
