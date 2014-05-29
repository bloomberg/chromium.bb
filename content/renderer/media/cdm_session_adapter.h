// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_keys.h"
#include "third_party/WebKit/public/platform/WebContentDecryptionModuleSession.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/pepper_cdm_wrapper.h"
#endif

class GURL;

namespace content {

#if defined(OS_ANDROID)
class RendererCdmManager;
#endif
class WebContentDecryptionModuleSessionImpl;

// Owns the CDM instance and makes calls from session objects to the CDM.
// Forwards the session ID-based callbacks of the MediaKeys interface to the
// appropriate session object. Callers should hold references to this class
// as long as they need the CDM instance.
class CdmSessionAdapter : public base::RefCounted<CdmSessionAdapter> {
 public:
  CdmSessionAdapter();

  // Returns true on success.
  bool Initialize(
#if defined(ENABLE_PEPPER_CDMS)
      const CreatePepperCdmCB& create_pepper_cdm_cb,
#elif defined(OS_ANDROID)
      RendererCdmManager* manager,
#endif
      const std::string& key_system,
      const GURL& security_origin);

  // Creates a new session and adds it to the internal map. The caller owns the
  // created session. RemoveSession() must be called when destroying it.
  WebContentDecryptionModuleSessionImpl* CreateSession(
      blink::WebContentDecryptionModuleSession::Client* client);

  // Removes a session from the internal map.
  void RemoveSession(uint32 session_id);

  // Initializes the session specified by |session_id| with the |content_type|
  // and |init_data| provided.
  void InitializeNewSession(uint32 session_id,
                            const std::string& content_type,
                            const uint8* init_data,
                            int init_data_length);

  // Updates the session specified by |session_id| with |response|.
  void UpdateSession(uint32 session_id,
                     const uint8* response,
                     int response_length);

  // Releases the session specified by |session_id|.
  void ReleaseSession(uint32 session_id);

  // Returns the Decryptor associated with this CDM. May be NULL if no
  // Decryptor is associated with the MediaKeys object.
  // TODO(jrummell): Figure out lifetimes, as WMPI may still use the decryptor
  // after WebContentDecryptionModule is freed. http://crbug.com/330324
  media::Decryptor* GetDecryptor();

#if defined(OS_ANDROID)
  // Returns the CDM ID associated with the |media_keys_|. May be kInvalidCdmId
  // if no CDM ID is associated.
  int GetCdmId() const;
#endif

 private:
  friend class base::RefCounted<CdmSessionAdapter>;
  typedef std::map<uint32, WebContentDecryptionModuleSessionImpl*> SessionMap;

  ~CdmSessionAdapter();

  // Callbacks for firing session events.
  void OnSessionCreated(uint32 session_id, const std::string& web_session_id);
  void OnSessionMessage(uint32 session_id,
                        const std::vector<uint8>& message,
                        const GURL& destination_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      uint32 system_code);

  // Helper function of the callbacks.
  WebContentDecryptionModuleSessionImpl* GetSession(uint32 session_id);

  // Session ID should be unique per renderer process for debugging purposes.
  static uint32 next_session_id_;

  scoped_ptr<media::MediaKeys> media_keys_;

  SessionMap sessions_;

#if defined(OS_ANDROID)
  int cdm_id_;
#endif

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<CdmSessionAdapter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CdmSessionAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_SESSION_ADAPTER_H_
