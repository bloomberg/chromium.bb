// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_
#define CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "media/base/browser_cdm.h"
#include "media/cdm/json_web_key.h"

namespace chromecast {
namespace media {
class DecryptContext;

// BrowserCdmCast is an extension of BrowserCdm that provides common
// functionality across CDM implementations.
// All these additional functions are synchronous so:
// - either both the CDM and the media pipeline must be running on the same
//   thread,
// - or BrowserCdmCast implementations must use some locks.
//
class BrowserCdmCast : public ::media::BrowserCdm {
 public:
  BrowserCdmCast();
  ~BrowserCdmCast() override;

  void SetCallbacks(
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::SessionErrorCB& session_error_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB& session_expiration_update_cb);

  // PlayerTracker implementation.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  // ::media::BrowserCdm implementation:
  void LoadSession(::media::MediaKeys::SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<::media::NewSessionCdmPromise> promise) override;
  ::media::CdmContext* GetCdmContext() override;

  // Returns the decryption context needed to decrypt frames encrypted with
  // |key_id|.
  // Returns null if |key_id| is not available.
  virtual scoped_refptr<DecryptContext> GetDecryptContext(
      const std::string& key_id) = 0;

 protected:
  void OnSessionMessage(const std::string& session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionKeysChange(const std::string& session_id,
                           const ::media::KeyIdAndKeyPairs& keys);

 private:
  ::media::SessionMessageCB session_message_cb_;
  ::media::SessionClosedCB session_closed_cb_;
  ::media::SessionErrorCB session_error_cb_;
  ::media::SessionKeysChangeCB session_keys_change_cb_;
  ::media::SessionExpirationUpdateCB session_expiration_update_cb_;

  base::Lock callback_lock_;
  uint32_t next_registration_id_;
  std::map<uint32_t, base::Closure> new_key_callbacks_;
  std::map<uint32_t, base::Closure> cdm_unset_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmCast);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_
