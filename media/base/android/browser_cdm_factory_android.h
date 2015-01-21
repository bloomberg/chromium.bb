// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BROWSER_CDM_FACTORY_ANDROID_H_
#define MEDIA_BASE_BROWSER_CDM_FACTORY_ANDROID_H_

#include "base/macros.h"
#include "media/base/browser_cdm_factory.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT BrowserCdmFactoryAndroid : public BrowserCdmFactory {
 public:
  BrowserCdmFactoryAndroid() {}
  ~BrowserCdmFactoryAndroid() final {};

  scoped_ptr<BrowserCdm> CreateBrowserCdm(
      const std::string& key_system,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionErrorCB& session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb) final;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCdmFactoryAndroid);
};

}  // namespace media

#endif  // MEDIA_BASE_BROWSER_CDM_FACTORY_ANDROID_H_
