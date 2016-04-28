// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_media_client_android.h"

ChromeMediaClientAndroid::ChromeMediaClientAndroid() {}

ChromeMediaClientAndroid::~ChromeMediaClientAndroid() {}

media::MediaDrmBridgeDelegate*
ChromeMediaClientAndroid::GetMediaDrmBridgeDelegate(
    const std::vector<uint8_t>& scheme_uuid) {
  if (scheme_uuid == widevine_delegate_.GetUUID())
    return &widevine_delegate_;
  return nullptr;
}
