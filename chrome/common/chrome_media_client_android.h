// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_MEDIA_CLIENT_ANDROID_H_
#define CHROME_COMMON_CHROME_MEDIA_CLIENT_ANDROID_H_

#include <stdint.h>

#include "base/macros.h"
#include "components/cdm/common/widevine_drm_delegate_android.h"
#include "media/base/android/media_client_android.h"

class ChromeMediaClientAndroid : public media::MediaClientAndroid {
 public:
  ChromeMediaClientAndroid();
  ~ChromeMediaClientAndroid() override;

 private:
  // media::MediaClientAndroid implementation:
  media::MediaDrmBridgeDelegate* GetMediaDrmBridgeDelegate(
      const std::vector<uint8_t>& scheme_uuid) override;

  cdm::WidevineDrmDelegateAndroid widevine_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMediaClientAndroid);
};

#endif  // CHROME_COMMON_CHROME_MEDIA_CLIENT_ANDROID_H_
