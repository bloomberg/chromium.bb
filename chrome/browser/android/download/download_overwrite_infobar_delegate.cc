// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_overwrite_infobar_delegate.h"

namespace chrome {
namespace android {

bool DownloadOverwriteInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

}  // namespace android
}  // namespace chrome
