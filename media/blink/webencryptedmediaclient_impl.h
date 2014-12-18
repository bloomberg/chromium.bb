// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
#define MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace media {

class MEDIA_EXPORT WebEncryptedMediaClientImpl
    : public blink::WebEncryptedMediaClient {
 public:
  WebEncryptedMediaClientImpl(scoped_ptr<CdmFactory> cdm_factory);
  virtual ~WebEncryptedMediaClientImpl();

  // WebEncryptedMediaClient implementation.
  virtual void requestMediaKeySystemAccess(
      blink::WebEncryptedMediaRequest request);

 private:
  scoped_ptr<CdmFactory> cdm_factory_;
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
