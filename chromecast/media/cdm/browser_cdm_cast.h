// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_
#define CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/threading/thread_checker.h"
#include "chromecast/public/media/cast_key_status.h"
#include "media/base/media_keys.h"
#include "media/base/player_tracker.h"
#include "media/cdm/json_web_key.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class PlayerTrackerImpl;
}

namespace chromecast {
namespace media {
class DecryptContextImpl;

// BrowserCdmCast is an extension of MediaKeys that provides common
// functionality across CDM implementations.
// All these additional functions are synchronous so:
// - either both the CDM and the media pipeline must be running on the same
//   thread,
// - or BrowserCdmCast implementations must use some locks.
//
class BrowserCdmCast : public ::media::MediaKeys,
                       public ::media::PlayerTracker {
 public:
  BrowserCdmCast();

  void Initialize(
      const ::media::SessionMessageCB& session_message_cb,
      const ::media::SessionClosedCB& session_closed_cb,
      const ::media::LegacySessionErrorCB& legacy_session_error_cb,
      const ::media::SessionKeysChangeCB& session_keys_change_cb,
      const ::media::SessionExpirationUpdateCB& session_expiration_update_cb);

  // ::media::PlayerTracker implementation.
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  // Returns the decryption context needed to decrypt frames encrypted with
  // |key_id|.
  // Returns null if |key_id| is not available.
  virtual scoped_ptr<DecryptContextImpl> GetDecryptContext(
      const std::string& key_id) const = 0;

  // Notifies that key status has changed (e.g. if expiry is detected by
  // hardware decoder).
  virtual void SetKeyStatus(const std::string& key_id,
                            CastKeyStatus key_status,
                            uint32_t system_code) = 0;

 protected:
  ~BrowserCdmCast() override;

  void OnSessionMessage(const std::string& session_id,
                        const std::vector<uint8_t>& message,
                        const GURL& destination_url,
                        ::media::MediaKeys::MessageType message_type);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionKeysChange(const std::string& session_id,
                           bool newly_usable_keys,
                           ::media::CdmKeysInfo keys_info);

  void KeyIdAndKeyPairsToInfo(const ::media::KeyIdAndKeyPairs& keys,
                              ::media::CdmKeysInfo* key_info);

 private:
  friend class BrowserCdmCastUi;

  // Allow subclasses to override to provide key sysytem specific
  // initialization.
  virtual void InitializeInternal();

  ::media::SessionMessageCB session_message_cb_;
  ::media::SessionClosedCB session_closed_cb_;
  ::media::LegacySessionErrorCB legacy_session_error_cb_;
  ::media::SessionKeysChangeCB session_keys_change_cb_;
  ::media::SessionExpirationUpdateCB session_expiration_update_cb_;

  scoped_ptr<::media::PlayerTrackerImpl> player_tracker_impl_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmCast);
};

// MediaKeys implementation that lives on the UI thread and forwards all calls
// to a BrowserCdmCast instance on the CMA thread. This is used to simplify the
// UI-CMA threading interaction.
class BrowserCdmCastUi : public ::media::MediaKeys {
 public:
  BrowserCdmCastUi(
      const scoped_refptr<BrowserCdmCast>& browser_cdm_cast,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);

  BrowserCdmCast* browser_cdm_cast() const;

 private:
  ~BrowserCdmCastUi() override;

  // ::media::MediaKeys implementation:
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate,
      scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      ::media::MediaKeys::SessionType session_type,
      ::media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      scoped_ptr<::media::NewSessionCdmPromise> promise) override;
  void LoadSession(::media::MediaKeys::SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<::media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<::media::SimpleCdmPromise> promise) override;

  scoped_refptr<BrowserCdmCast> browser_cdm_cast_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmCastUi);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_
