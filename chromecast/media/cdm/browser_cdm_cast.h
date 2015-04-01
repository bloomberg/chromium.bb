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
#include "base/threading/thread_checker.h"
#include "media/base/browser_cdm.h"
#include "media/cdm/json_web_key.h"

namespace base {
class MessageLoopProxy;
}

namespace media {
class PlayerTrackerImpl;
}

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

  void Initialize(
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
      const std::string& key_id) const = 0;

 protected:
  void OnSessionMessage(const std::string& session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionKeysChange(const std::string& session_id,
                           const ::media::KeyIdAndKeyPairs& keys);

 private:
  friend class BrowserCdmCastUi;

  // Helper methods for forwarding calls to methods that take raw pointers.
  void SetServerCertificateHelper(
      const std::vector<uint8>& certificate_data,
      scoped_ptr<::media::SimpleCdmPromise> promise);
  void CreateSessionAndGenerateRequestHelper(
      ::media::MediaKeys::SessionType session_type,
      const std::string& init_data_type,
      const std::vector<uint8>& init_data,
      scoped_ptr<::media::NewSessionCdmPromise> promise);
  void UpdateSessionHelper(const std::string& session_id,
                           const std::vector<uint8>& response,
                           scoped_ptr<::media::SimpleCdmPromise> promise);

  ::media::SessionMessageCB session_message_cb_;
  ::media::SessionClosedCB session_closed_cb_;
  ::media::SessionErrorCB session_error_cb_;
  ::media::SessionKeysChangeCB session_keys_change_cb_;
  ::media::SessionExpirationUpdateCB session_expiration_update_cb_;

  scoped_ptr<::media::PlayerTrackerImpl> player_tracker_impl_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmCast);
};

// BrowserCdm implementation that lives on the UI thread and forwards all calls
// to a BrowserCdmCast instance on the CMA thread. This is used to simplify the
// UI-CMA threading interaction.
class BrowserCdmCastUi : public ::media::BrowserCdm {
 public:
  BrowserCdmCastUi(
      scoped_ptr<BrowserCdmCast> browser_cdm_cast,
      const scoped_refptr<base::MessageLoopProxy>& cdm_loop);
  ~BrowserCdmCastUi() override;

  // PlayerTracker implementation:
  int RegisterPlayer(const base::Closure& new_key_cb,
                     const base::Closure& cdm_unset_cb) override;
  void UnregisterPlayer(int registration_id) override;

  BrowserCdmCast* browser_cdm_cast() const;

 private:
  // ::media::MediaKeys implementation:
  void SetServerCertificate(
      const uint8* certificate_data,
      int certificate_data_length,
      scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      ::media::MediaKeys::SessionType session_type,
      const std::string& init_data_type,
      const uint8* init_data,
      int init_data_length,
      scoped_ptr<::media::NewSessionCdmPromise> promise) override;
  void LoadSession(::media::MediaKeys::SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<::media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const uint8* response,
                     int response_length,
                     scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<::media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<::media::SimpleCdmPromise> promise) override;
  ::media::CdmContext* GetCdmContext() override;

  scoped_ptr<BrowserCdmCast> browser_cdm_cast_;
  scoped_refptr<base::MessageLoopProxy> cdm_loop_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCdmCastUi);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CDM_BROWSER_CDM_CAST_H_
