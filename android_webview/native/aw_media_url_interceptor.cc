// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "android_webview/common/url_constants.h"
#include "android_webview/native/aw_assets.h"
#include "android_webview/native/aw_media_url_interceptor.h"
#include "base/strings/string_util.h"
#include "content/public/common/url_constants.h"

namespace android_webview {

bool AwMediaUrlInterceptor::Intercept(const std::string& url,
              int* fd, int64* offset, int64* size) const{
  const std::string asset_file_prefix(
      std::string(url::kFileScheme) +
      std::string(url::kStandardSchemeSeparator) +
      android_webview::kAndroidAssetPath);

  if (StartsWithASCII(url, asset_file_prefix, true)) {
    std::string filename(url);
    ReplaceFirstSubstringAfterOffset(&filename, 0, asset_file_prefix, "");
    return AwAssets::OpenAsset(filename, fd, offset, size);
  }

  return false;
}

}  // namespace android_webview

