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

// Forwards the reference ID-based callbacks of the MediaKeys interface to the
// appropriate session object.
class ReferenceIdAdapter {
 public:
  ReferenceIdAdapter();
  ~ReferenceIdAdapter();

  // On success, creates a MediaKeys, returns it in |media_keys|, returns true.
  bool Initialize(const std::string& key_system,
                  scoped_ptr<media::MediaKeys>* media_keys);

  // Adds a session to the internal map. Does not take ownership of the session.
  void AddSession(uint32 reference_id,
                  WebContentDecryptionModuleSessionImpl* session);

  // Removes a session from the internal map.
  void RemoveSession(uint32 reference_id);

 private:
  typedef std::map<uint32, WebContentDecryptionModuleSessionImpl*> SessionMap;

  // Callbacks for firing key events.
  void KeyAdded(uint32 reference_id);
  void KeyError(uint32 reference_id,
                media::MediaKeys::KeyError error_code,
                int system_code);
  void KeyMessage(uint32 reference_id,
                  const std::vector<uint8>& message,
                  const std::string& destination_url);
  void SetSessionId(uint32 reference_id, const std::string& session_id);

  // Helper function of the callbacks.
  WebContentDecryptionModuleSessionImpl* GetSession(uint32 reference_id);

  base::WeakPtrFactory<ReferenceIdAdapter> weak_ptr_factory_;

  SessionMap sessions_;

  DISALLOW_COPY_AND_ASSIGN(ReferenceIdAdapter);
};

ReferenceIdAdapter::ReferenceIdAdapter()
    : weak_ptr_factory_(this) {
}

ReferenceIdAdapter::~ReferenceIdAdapter() {
}

bool ReferenceIdAdapter::Initialize(const std::string& key_system,
                                    scoped_ptr<media::MediaKeys>* media_keys) {
  DCHECK(media_keys);
  DCHECK(!*media_keys);

  base::WeakPtr<ReferenceIdAdapter> weak_this = weak_ptr_factory_.GetWeakPtr();
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
          base::Bind(&ReferenceIdAdapter::KeyAdded, weak_this),
          base::Bind(&ReferenceIdAdapter::KeyError, weak_this),
          base::Bind(&ReferenceIdAdapter::KeyMessage, weak_this),
          base::Bind(&ReferenceIdAdapter::SetSessionId, weak_this));
  if (!created_media_keys)
    return false;

  *media_keys = created_media_keys.Pass();
  return true;
}

void ReferenceIdAdapter::AddSession(
    uint32 reference_id,
    WebContentDecryptionModuleSessionImpl* session) {
  DCHECK(sessions_.find(reference_id) == sessions_.end());
  sessions_[reference_id] = session;
}

void ReferenceIdAdapter::RemoveSession(uint32 reference_id) {
  DCHECK(sessions_.find(reference_id) != sessions_.end());
  sessions_.erase(reference_id);
}

void ReferenceIdAdapter::KeyAdded(uint32 reference_id) {
  GetSession(reference_id)->KeyAdded();
}

void ReferenceIdAdapter::KeyError(uint32 reference_id,
                                  media::MediaKeys::KeyError error_code,
                                  int system_code) {
  GetSession(reference_id)->KeyError(error_code, system_code);
}

void ReferenceIdAdapter::KeyMessage(uint32 reference_id,
                                    const std::vector<uint8>& message,
                                    const std::string& destination_url) {
  GetSession(reference_id)->KeyMessage(message, destination_url);
}

void ReferenceIdAdapter::SetSessionId(uint32 reference_id,
                                      const std::string& session_id) {
  GetSession(reference_id)->SetSessionId(session_id);
}

WebContentDecryptionModuleSessionImpl* ReferenceIdAdapter::GetSession(
    uint32 reference_id) {
  DCHECK(sessions_.find(reference_id) != sessions_.end());
  return sessions_[reference_id];
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

  // ReferenceIdAdapter creates the MediaKeys so it can provide its callbacks to
  // during creation of the MediaKeys.
  scoped_ptr<media::MediaKeys> media_keys;
  scoped_ptr<ReferenceIdAdapter> adapter(new ReferenceIdAdapter());
  if (!adapter->Initialize(UTF16ToASCII(key_system), &media_keys))
    return NULL;

  return new WebContentDecryptionModuleImpl(media_keys.Pass(), adapter.Pass());
}

WebContentDecryptionModuleImpl::WebContentDecryptionModuleImpl(
    scoped_ptr<media::MediaKeys> media_keys,
    scoped_ptr<ReferenceIdAdapter> adapter)
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
  WebContentDecryptionModuleSessionImpl* session =
      new WebContentDecryptionModuleSessionImpl(
          media_keys_.get(),
          client,
          base::Bind(&WebContentDecryptionModuleImpl::OnSessionClosed,
                     base::Unretained(this)));

  adapter_->AddSession(session->reference_id(), session);
  return session;
}

void WebContentDecryptionModuleImpl::OnSessionClosed(uint32 reference_id) {
  adapter_->RemoveSession(reference_id);
}

}  // namespace content
