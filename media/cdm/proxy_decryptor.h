// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PROXY_DECRYPTOR_H_
#define MEDIA_CDM_PROXY_DECRYPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace media {

class CdmFactory;
class MediaPermission;

// ProxyDecryptor is for EME v0.1b only. It should not be used for the WD API.
// A decryptor proxy that creates a real decryptor object on demand and
// forwards decryptor calls to it.
//
// TODO(xhwang): Currently we don't support run-time switching among decryptor
// objects. Fix this when needed.
// TODO(xhwang): The ProxyDecryptor is not a Decryptor. Find a better name!
class MEDIA_EXPORT ProxyDecryptor {
 public:
  // These are similar to the callbacks in media_keys.h, but pass back the
  // session ID rather than the internal session ID.
  typedef base::Callback<void(const std::string& session_id)> KeyAddedCB;
  typedef base::Callback<void(const std::string& session_id,
                              MediaKeys::KeyError error_code,
                              uint32 system_code)> KeyErrorCB;
  typedef base::Callback<void(const std::string& session_id,
                              const std::vector<uint8>& message,
                              const GURL& destination_url)> KeyMessageCB;

  ProxyDecryptor(MediaPermission* media_permission,
                 const KeyAddedCB& key_added_cb,
                 const KeyErrorCB& key_error_cb,
                 const KeyMessageCB& key_message_cb);
  virtual ~ProxyDecryptor();

  // Returns the CdmContext associated with this object.
  CdmContext* GetCdmContext();

  // Only call this once.
  bool InitializeCDM(CdmFactory* cdm_factory,
                     const std::string& key_system,
                     const GURL& security_origin);

  // May only be called after InitializeCDM() succeeds.
  bool GenerateKeyRequest(const std::string& init_data_type,
                          const uint8* init_data,
                          int init_data_length);
  void AddKey(const uint8* key, int key_length,
              const uint8* init_data, int init_data_length,
              const std::string& session_id);
  void CancelKeyRequest(const std::string& session_id);

 private:
  // Helper function to create MediaKeys to handle the given |key_system|.
  scoped_ptr<MediaKeys> CreateMediaKeys(
      CdmFactory* cdm_factory,
      const std::string& key_system,
      const GURL& security_origin);

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 const base::Time& new_expiry_time);
  void GenerateKeyAdded(const std::string& session_id);
  void OnSessionClosed(const std::string& session_id);
  void OnSessionError(const std::string& session_id,
                      MediaKeys::Exception exception_code,
                      uint32 system_code,
                      const std::string& error_message);

  // Callback for permission request.
  void OnPermissionStatus(MediaKeys::SessionType session_type,
                          const std::string& init_data_type,
                          const std::vector<uint8>& init_data,
                          scoped_ptr<NewSessionCdmPromise> promise,
                          bool granted);

  enum SessionCreationType {
    TemporarySession,
    PersistentSession,
    LoadSession
  };

  // Called when a session is actually created or loaded.
  void SetSessionId(SessionCreationType session_type,
                    const std::string& session_id);

  // The real MediaKeys that manages key operations for the ProxyDecryptor.
  scoped_ptr<MediaKeys> media_keys_;

  MediaPermission* media_permission_;

  // Callbacks for firing key events.
  KeyAddedCB key_added_cb_;
  KeyErrorCB key_error_cb_;
  KeyMessageCB key_message_cb_;

  std::string key_system_;
  GURL security_origin_;

  // Keep track of both persistent and non-persistent sessions.
  base::hash_map<std::string, bool> active_sessions_;

  bool is_clear_key_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ProxyDecryptor> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyDecryptor);
};

}  // namespace media

#endif  // MEDIA_CDM_PROXY_DECRYPTOR_H_
