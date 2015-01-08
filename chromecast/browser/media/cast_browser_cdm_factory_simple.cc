// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

namespace chromecast {
namespace media {

scoped_ptr< ::media::BrowserCdm> CreatePlatformBrowserCdm(
    const CastKeySystem& key_system,
    const ::media::BrowserCdm::SessionCreatedCB& session_created_cb,
    const ::media::BrowserCdm::SessionMessageCB& session_message_cb,
    const ::media::BrowserCdm::SessionReadyCB& session_ready_cb,
    const ::media::BrowserCdm::SessionClosedCB& session_closed_cb,
    const ::media::BrowserCdm::SessionErrorCB& session_error_cb) {
  return scoped_ptr< ::media::BrowserCdm>();
}

}  // namespace media
}  // namespace chromecast
