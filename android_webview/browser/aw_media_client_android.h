// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_MEDIA_CLIENT_ANDROID_H_
#define ANDROID_WEBVIEW_BROWSER_AW_MEDIA_CLIENT_ANDROID_H_

#include "base/macros.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_client_android.h"

namespace android_webview {

class AwMediaClientAndroid : public media::MediaClientAndroid {
 public:
  // |key_system_uuid_mappings| is a list of strings containing key-system/UUID
  // pairs, in the format "key system name,UUID as string".
  explicit AwMediaClientAndroid(
      const std::vector<std::string>& key_system_uuid_mappings);
  ~AwMediaClientAndroid() override;

 private:
  // media::MediaClientAndroid implementation:
  void AddKeySystemUUIDMappings(KeySystemUuidMap* map) override;
  media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const media::UUID& scheme_uuid) override;

  std::vector<std::string> key_system_uuid_mappings_;
  cdm::WidevineDrmDelegateAndroid widevine_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwMediaClientAndroid);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_MEDIA_CLIENT_ANDROID_H_
