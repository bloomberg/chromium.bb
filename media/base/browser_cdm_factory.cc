// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/browser_cdm_factory.h"

#include "base/logging.h"

#if defined(OS_ANDROID)
#include "media/base/android/browser_cdm_factory_android.h"
#endif

namespace media {

namespace {
BrowserCdmFactory* g_cdm_factory = NULL;
}

void SetBrowserCdmFactory(BrowserCdmFactory* factory) {
  DCHECK(!g_cdm_factory);
  g_cdm_factory = factory;
}

scoped_ptr<BrowserCdm> CreateBrowserCdm(
    const std::string& key_system,
    const BrowserCdm::SessionCreatedCB& session_created_cb,
    const BrowserCdm::SessionMessageCB& session_message_cb,
    const BrowserCdm::SessionReadyCB& session_ready_cb,
    const BrowserCdm::SessionClosedCB& session_closed_cb,
    const BrowserCdm::SessionErrorCB& session_error_cb) {
  if (!g_cdm_factory) {
#if defined(OS_ANDROID)
    SetBrowserCdmFactory(new BrowserCdmFactoryAndroid);
#else
    LOG(ERROR) << "Cannot create BrowserCdm: no BrowserCdmFactory available!";
    return scoped_ptr<BrowserCdm>();
#endif
  }

  return g_cdm_factory->CreateBrowserCdm(key_system,
                                         session_created_cb,
                                         session_message_cb,
                                         session_ready_cb,
                                         session_closed_cb,
                                         session_error_cb);
}

}  // namespace media
