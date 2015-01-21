// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_BROWSER_CDM_FACTORY_H_
#define MEDIA_BASE_BROWSER_CDM_FACTORY_H_

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/browser_cdm.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT BrowserCdmFactory {
 public:
  BrowserCdmFactory() {}
  virtual ~BrowserCdmFactory() {}

  virtual scoped_ptr<BrowserCdm> CreateBrowserCdm(
      const std::string& key_system,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionErrorCB& session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserCdmFactory);
};

// Provides a factory for creating BrowserCdm instances. There is only one
// BrowserCdmFactory per process.
void SetBrowserCdmFactory(BrowserCdmFactory* factory);

// Creates a BrowserCdm for |key_system|. Returns NULL if the CDM cannot be
// created.
// TODO(xhwang): Add ifdef for IPC based CDM.
scoped_ptr<BrowserCdm> MEDIA_EXPORT
CreateBrowserCdm(const std::string& key_system,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionErrorCB& session_error_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb);

}  // namespace media

#endif  // MEDIA_BASE_BROWSER_CDM_FACTORY_H_
