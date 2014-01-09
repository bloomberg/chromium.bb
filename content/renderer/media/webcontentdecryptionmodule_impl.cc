// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webcontentdecryptionmodule_impl.h"

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "content/renderer/media/crypto/content_decryption_module_factory.h"
#include "content/renderer/media/webcontentdecryptionmodulesession_impl.h"
#include "media/base/media_keys.h"
#include "url/gurl.h"

namespace content {

// Forwards the session ID-based callbacks of the MediaKeys interface to the
// appropriate session object.
class SessionIdAdapter {
 public:
  SessionIdAdapter();
  ~SessionIdAdapter();

  // On success, creates a MediaKeys, returns it in |media_keys|, returns true.
  bool Initialize(const std::string& key_system,
                  scoped_ptr<media::MediaKeys>* media_keys);

  // Generates a unique internal session id.
  uint32 GenerateSessionId();

  // Adds a session to the internal map. Does not take ownership of the session.
  void AddSession(uint32 session_id,
                  WebContentDecryptionModuleSessionImpl* session);

  // Removes a session from the internal map.
  void RemoveSession(uint32 session_id);

 private:
  typedef std::map<uint32, WebContentDecryptionModuleSessionImpl*> SessionMap;

  // Callbacks for firing session events.
  void OnSessionCreated(uint32 session_id, const std::string& web_session_id);
  void OnSessionMessage(uint32 session_id,
                        const std::vector<uint8>& message,
                        const std::string& destination_url);
  void OnSessionReady(uint32 session_id);
  void OnSessionClosed(uint32 session_id);
  void OnSessionError(uint32 session_id,
                      media::MediaKeys::KeyError error_code,
                      int system_code);

  // Helper function of the callbacks.
  WebContentDecryptionModuleSessionImpl* GetSession(uint32 session_id);

  base::WeakPtrFactory<SessionIdAdapter> weak_ptr_factory_;

  SessionMap sessions_;

  // Session ID should be unique per renderer process for debugging purposes.
  static uint32 next_session_id_;

  DISALLOW_COPY_AND_ASSIGN(SessionIdAdapter);
};

const uint32 kStartingSessionId = 1;
uint32 SessionIdAdapter::next_session_id_ = kStartingSessionId;
COMPILE_ASSERT(kStartingSessionId > media::MediaKeys::kInvalidSessionId,
               invalid_starting_value);

SessionIdAdapter::SessionIdAdapter()
    : weak_ptr_factory_(this) {
}

SessionIdAdapter::~SessionIdAdapter() {
}

bool SessionIdAdapter::Initialize(const std::string& key_system,
                                  scoped_ptr<media::MediaKeys>* media_keys) {
  DCHECK(media_keys);
  DCHECK(!*media_keys);

  base::WeakPtr<SessionIdAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();
  scoped_ptr<media::MediaKeys> created_media_keys =
      ContentDecryptionModuleFactory::Create(
          // TODO(ddorwin): Address lower in the stack: http://crbug.com/252065
          "webkit-" + key_system,
#if defined(ENABLE_PEPPER_CDMS)
          // TODO(ddorwin): Support Pepper-based CDMs: http://crbug.com/250049
          NULL,
          NULL,
          base::Closure(),
#elif defined(OS_ANDROID)
          // TODO(xhwang): Support Android.
          NULL,
          0,
          // TODO(ddorwin): Get the URL for the frame containing the MediaKeys.
          GURL(),
#endif  // defined(ENABLE_PEPPER_CDMS)
          base::Bind(&SessionIdAdapter::OnSessionCreated, weak_this),
          base::Bind(&SessionIdAdapter::OnSessionMessage, weak_this),
          base::Bind(&SessionIdAdapter::OnSessionReady, weak_this),
          base::Bind(&SessionIdAdapter::OnSessionClosed, weak_this),
          base::Bind(&SessionIdAdapter::OnSessionError, weak_this));
  if (!created_media_keys)
    return false;

  *media_keys = created_media_keys.Pass();
  return true;
}

uint32 SessionIdAdapter::GenerateSessionId() {
  return next_session_id_++;
}

void SessionIdAdapter::AddSession(
    uint32 session_id,
    WebContentDecryptionModuleSessionImpl* session) {
  DCHECK(sessions_.find(session_id) == sessions_.end());
  sessions_[session_id] = session;
}

void SessionIdAdapter::RemoveSession(uint32 session_id) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  sessions_.erase(session_id);
}

void SessionIdAdapter::OnSessionCreated(uint32 session_id,
                                        const std::string& web_session_id) {
  GetSession(session_id)->OnSessionCreated(web_session_id);
}

void SessionIdAdapter::OnSessionMessage(uint32 session_id,
                                        const std::vector<uint8>& message,
                                        const std::string& destination_url) {
  GetSession(session_id)->OnSessionMessage(message, destination_url);
}

void SessionIdAdapter::OnSessionReady(uint32 session_id) {
  GetSession(session_id)->OnSessionReady();
}

void SessionIdAdapter::OnSessionClosed(uint32 session_id) {
  GetSession(session_id)->OnSessionClosed();
}

void SessionIdAdapter::OnSessionError(uint32 session_id,
                                      media::MediaKeys::KeyError error_code,
                                      int system_code) {
  GetSession(session_id)->OnSessionError(error_code, system_code);
}

WebContentDecryptionModuleSessionImpl* SessionIdAdapter::GetSession(
    uint32 session_id) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  return sessions_[session_id];
}

//------------------------------------------------------------------------------

WebContentDecryptionModuleImpl*
WebContentDecryptionModuleImpl::Create(const base::string16& key_system) {
  // TODO(ddorwin): Guard against this in supported types check and remove this.
  // Chromium only supports ASCII key systems.
  if (!IsStringASCII(key_system)) {
    NOTREACHED();
    return NULL;
  }

  // SessionIdAdapter creates the MediaKeys so it can provide its callbacks to
  // during creation of the MediaKeys.
  scoped_ptr<media::MediaKeys> media_keys;
  scoped_ptr<SessionIdAdapter> adapter(new SessionIdAdapter());
  if (!adapter->Initialize(UTF16ToASCII(key_system), &media_keys))
    return NULL;

  return new WebContentDecryptionModuleImpl(media_keys.Pass(), adapter.Pass());
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    scoped_ptr<media::MediaKeys> media_keys,
    scoped_ptr<SessionIdAdapter> adapter)
    : media_keys_(media_keys.Pass()),
      adapter_(adapter.Pass()) {
}

WebContentDecryptionModuleImpl::~WebContentDecryptionModuleImpl() {
}

// The caller owns the created session.
blink::WebContentDecryptionModuleSession*
WebContentDecryptionModuleImpl::createSession(
    blink::WebContentDecryptionModuleSession::Client* client) {
  DCHECK(media_keys_);
  uint32 session_id = adapter_->GenerateSessionId();
  WebContentDecryptionModuleSessionImpl* session =
      new WebContentDecryptionModuleSessionImpl(
          session_id,
          media_keys_.get(),
          client,
          base::Bind(&WebContentDecryptionModuleImpl::OnSessionClosed,
                     base::Unretained(this)));

  adapter_->AddSession(session_id, session);
  return session;
}

media::Decryptor* WebContentDecryptionModuleImpl::GetDecryptor() {
  return media_keys_->GetDecryptor();
}

void WebContentDecryptionModuleImpl::OnSessionClosed(uint32 session_id) {
  adapter_->RemoveSession(session_id);
}

}  // namespace content
