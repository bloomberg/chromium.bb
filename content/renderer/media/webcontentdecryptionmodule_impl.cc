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

  // Adds a session to the internal map. Does not take ownership of the session.
  void AddSession(const std::string& session_id,
                  WebContentDecryptionModuleSessionImpl* session);

  // Removes a session from the internal map.
  void RemoveSession(const std::string& session_id);

 private:
  typedef std::map<std::string, WebContentDecryptionModuleSessionImpl*>
      SessionMap;

  // Callbacks for firing key events.
  void KeyAdded(const std::string& session_id);
  void KeyError(const std::string& session_id,
                media::MediaKeys::KeyError error_code,
                int system_code);
  void KeyMessage(const std::string& session_id,
                  const std::vector<uint8>& message,
                  const std::string& destination_url);

  // Helper function of the callbacks.
  WebContentDecryptionModuleSessionImpl* GetSession(
      const std::string& session_id);

  base::WeakPtrFactory<SessionIdAdapter> weak_ptr_factory_;

  SessionMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(SessionIdAdapter);
};

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
#endif  // defined(ENABLE_PEPPER_CDMS)
          base::Bind(&SessionIdAdapter::KeyAdded, weak_this),
          base::Bind(&SessionIdAdapter::KeyError, weak_this),
          base::Bind(&SessionIdAdapter::KeyMessage, weak_this));
  if (!created_media_keys)
    return false;

  *media_keys = created_media_keys.Pass();
  return true;
}

void SessionIdAdapter::AddSession(
    const std::string& session_id,
    WebContentDecryptionModuleSessionImpl* session) {
  DCHECK(sessions_.find(session_id) == sessions_.end());
  sessions_[session_id] = session;
}

void SessionIdAdapter::RemoveSession(const std::string& session_id) {
  DCHECK(sessions_.find(session_id) != sessions_.end());
  sessions_.erase(session_id);
}

void SessionIdAdapter::KeyAdded(const std::string& session_id) {
  GetSession(session_id)->KeyAdded();
}

void SessionIdAdapter::KeyError(const std::string& session_id,
                                media::MediaKeys::KeyError error_code,
                                int system_code) {
  GetSession(session_id)->KeyError(error_code, system_code);
}

void SessionIdAdapter::KeyMessage(const std::string& session_id,
                                  const std::vector<uint8>& message,
                                  const std::string& destination_url) {
  GetSession(session_id)->KeyMessage(message, destination_url);
}

WebContentDecryptionModuleSessionImpl* SessionIdAdapter::GetSession(
    const std::string& session_id) {
  // TODO(ddorwin): Map session IDs correctly. For now, we only support one.
  std::string session_object_id = "";
  WebContentDecryptionModuleSessionImpl* session = sessions_[session_object_id];
  DCHECK(session); // It must have been present.
  return session;
}

//------------------------------------------------------------------------------

WebContentDecryptionModuleImpl*
WebContentDecryptionModuleImpl::Create(const string16& key_system) {
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
WebKit::WebContentDecryptionModuleSession*
WebContentDecryptionModuleImpl::createSession(
    WebKit::WebContentDecryptionModuleSession::Client* client) {
  DCHECK(media_keys_);
  WebContentDecryptionModuleSessionImpl* session =
      new WebContentDecryptionModuleSessionImpl(
          media_keys_.get(),
          client,
          base::Bind(&WebContentDecryptionModuleImpl::OnSessionClosed,
                     base::Unretained(this)));

  // TODO(ddorwin): session_id is not populated yet!
  adapter_->AddSession(session->session_id(), session);
  return session;
}

void WebContentDecryptionModuleImpl::OnSessionClosed(
    const std::string& session_id) {
  adapter_->RemoveSession(session_id);
}

}  // namespace content
