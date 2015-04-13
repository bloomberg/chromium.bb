// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_FACTORY_H_
#define MEDIA_BASE_CDM_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"

class GURL;

namespace media {

class MEDIA_EXPORT CdmFactory {
 public:
  using CdmCreatedCB = base::Callback<void(scoped_ptr<MediaKeys>)>;

  CdmFactory();
  virtual ~CdmFactory();

  // Creates a CDM for |key_system| and returns it through |cdm_created_cb|
  // asynchronously.
  virtual void Create(
      const std::string& key_system,
      bool allow_distinctive_identifier,
      bool allow_persistent_state,
      const GURL& security_origin,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const LegacySessionErrorCB& legacy_session_error_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb,
      const CdmCreatedCB& cdm_created_cb) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmFactory);
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_FACTORY_H_
