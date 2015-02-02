// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CAST_BROWSER_CDM_FACTORY_H_
#define CHROMECAST_BROWSER_MEDIA_CAST_BROWSER_CDM_FACTORY_H_

#include "chromecast/media/base/key_systems_common.h"
#include "media/base/browser_cdm.h"
#include "media/base/browser_cdm_factory.h"

namespace chromecast {
namespace media {

class BrowserCdmCast;

class CastBrowserCdmFactory : public ::media::BrowserCdmFactory {
 public:
  CastBrowserCdmFactory() {}
  ~CastBrowserCdmFactory() override {};

  // ::media::BrowserCdmFactory implementation:
  scoped_ptr< ::media::BrowserCdm> CreateBrowserCdm(
      const std::string& key_system,
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::SessionErrorCB& session_error_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB&
          session_expiration_update_cb) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastBrowserCdmFactory);
};

// Allow platform-specific CDMs to be provided.
scoped_ptr<BrowserCdmCast> CreatePlatformBrowserCdm(
    const CastKeySystem& key_system);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_CAST_BROWSER_CDM_FACTORY_H_
