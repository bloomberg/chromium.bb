// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
#define MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_

#include <string>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_export.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleResult.h"
#include "third_party/WebKit/public/platform/WebEncryptedMediaClient.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

namespace media {

class MediaPermission;

class MEDIA_EXPORT WebEncryptedMediaClientImpl
    : public blink::WebEncryptedMediaClient {
 public:
  WebEncryptedMediaClientImpl(scoped_ptr<CdmFactory> cdm_factory,
                              MediaPermission* media_permission);
  virtual ~WebEncryptedMediaClientImpl();

  // WebEncryptedMediaClient implementation.
  virtual void requestMediaKeySystemAccess(
      blink::WebEncryptedMediaRequest request);

  // Create the CDM for |key_system| and |security_origin|. The caller owns
  // the created cdm (passed back using |result|).
  void CreateCdm(const blink::WebString& key_system,
                 bool allow_distinctive_identifier,
                 bool allow_persistent_state,
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebContentDecryptionModuleResult result);

 private:
  // Pick a supported configuration if possible, and complete the request. This
  // method may asynchronously invoke itself after prompting for permissions.
  void SelectSupportedConfiguration(blink::WebEncryptedMediaRequest request,
                                    bool was_permission_requested,
                                    bool is_permission_granted);

  // Report usage of key system to UMA. There are 2 different counts logged:
  // 1. The key system is requested.
  // 2. The requested key system and options are supported.
  // Each stat is only reported once per renderer frame per key system.
  class Reporter;

  // Gets the Reporter for |key_system|. If it doesn't already exist,
  // create one.
  Reporter* GetReporter(const std::string& key_system);

  // Key system <-> Reporter map.
  typedef base::ScopedPtrHashMap<std::string, Reporter> Reporters;
  Reporters reporters_;

  scoped_ptr<CdmFactory> cdm_factory_;
  MediaPermission* media_permission_;

  base::WeakPtrFactory<WebEncryptedMediaClientImpl> weak_factory_;
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBENCRYPTEDMEDIACLIENT_IMPL_H_
