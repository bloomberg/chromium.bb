// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/disk_image_type_sniffer_mac.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

namespace {

const uint8_t kKolySignature[4] = {'k', 'o', 'l', 'y'};
constexpr size_t kSizeKolySignatureInBytes = sizeof(kKolySignature);
const size_t kSizeKolyTrailerInBytes = 512;

}  // namespace

DiskImageTypeSnifferMac::DiskImageTypeSnifferMac() {}

// static
bool DiskImageTypeSnifferMac::IsAppleDiskImage(const base::FilePath& dmg_file) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);

  base::File file(dmg_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;

  char data[kSizeKolySignatureInBytes];

  if (file.Seek(base::File::FROM_END, -1 * kSizeKolyTrailerInBytes) == -1)
    return false;

  if (file.ReadAtCurrentPos(data, kSizeKolySignatureInBytes) !=
      kSizeKolySignatureInBytes)
    return false;

  return (memcmp(data, kKolySignature, kSizeKolySignatureInBytes) == 0);
}

DiskImageTypeSnifferMac::~DiskImageTypeSnifferMac() = default;

}  // namespace safe_browsing
