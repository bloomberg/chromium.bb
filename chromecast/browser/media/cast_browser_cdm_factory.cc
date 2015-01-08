// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

namespace chromecast {
namespace media {

scoped_ptr< ::media::BrowserCdm> CastBrowserCdmFactory::CreateBrowserCdm(
    const std::string& key_system_name,
    const ::media::BrowserCdm::SessionCreatedCB& session_created_cb,
    const ::media::BrowserCdm::SessionMessageCB& session_message_cb,
    const ::media::BrowserCdm::SessionReadyCB& session_ready_cb,
    const ::media::BrowserCdm::SessionClosedCB& session_closed_cb,
    const ::media::BrowserCdm::SessionErrorCB& session_error_cb) {
  CastKeySystem key_system(GetKeySystemByName(key_system_name));

  // TODO(gunsch): handle ClearKey decryption. See crbug.com/441957

  scoped_ptr< ::media::BrowserCdm> platform_cdm(
      CreatePlatformBrowserCdm(key_system,
                               session_created_cb,
                               session_message_cb,
                               session_ready_cb,
                               session_closed_cb,
                               session_error_cb));
  if (platform_cdm) {
    return platform_cdm.Pass();
  }

  LOG(INFO) << "No matching key system found.";
  return scoped_ptr< ::media::BrowserCdm>();
}

}  // namespace media
}  // namespace chromecast
