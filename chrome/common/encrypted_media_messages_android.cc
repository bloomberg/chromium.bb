// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/encrypted_media_messages_android.h"

namespace android {

SupportedKeySystemRequest::SupportedKeySystemRequest()
    : codecs(NO_SUPPORTED_CODECS) {}

SupportedKeySystemRequest::~SupportedKeySystemRequest() {}

SupportedKeySystemResponse::SupportedKeySystemResponse()
    : compositing_codecs(NO_SUPPORTED_CODECS),
      non_compositing_codecs(NO_SUPPORTED_CODECS) {}

SupportedKeySystemResponse::~SupportedKeySystemResponse() {}

}  // namespace android
