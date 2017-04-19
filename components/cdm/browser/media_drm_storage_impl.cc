// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/media_drm_storage_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_handle.h"

// The storage will be managed by PrefService. All data will be stored in a
// dictionary under the key "media.media_drm_storage". The dictionary is
// structured as follows:
//
// {
//     $origin: {
//         "origin_id": $origin_id
//         "creation_time": $creation_time
//         "sessions" : {
//             $session_id: {
//                 "key_set_id": $key_set_id,
//                 "mime_type": $mime_type,
//                 "creation_time": $creation_time
//             },
//             # more session_id map...
//         }
//     },
//     # more origin map...
// }

namespace cdm {

namespace {

const char kMediaDrmStorage[] = "media.media_drm_storage";
const char kCreationTime[] = "creation_time";
const char kSessions[] = "sessions";
const char kKeySetId[] = "key_set_id";
const char kMimeType[] = "mime_type";

std::unique_ptr<base::DictionaryValue> CreateOriginDictionary() {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  // TODO(xhwang): Create |origin_id|.
  dict->SetDouble(kCreationTime, base::Time::Now().ToDoubleT());
  return dict;
}

std::unique_ptr<base::DictionaryValue> CreateSessionDictionary(
    const std::vector<uint8_t>& key_set_id,
    const std::string& mime_type) {
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetString(kKeySetId,
                  std::string(reinterpret_cast<const char*>(key_set_id.data()),
                              key_set_id.size()));
  dict->SetString(kMimeType, mime_type);
  dict->SetDouble(kCreationTime, base::Time::Now().ToDoubleT());
  return dict;
}

bool GetSessionData(const base::DictionaryValue* sesssion_dict,
                    std::vector<uint8_t>* key_set_id,
                    std::string* mime_type) {
  std::string key_set_id_string;
  if (!sesssion_dict->GetString(kKeySetId, &key_set_id_string))
    return false;

  if (!sesssion_dict->GetString(kMimeType, mime_type))
    return false;

  key_set_id->assign(key_set_id_string.begin(), key_set_id_string.end());
  return true;
}

#if DCHECK_IS_ON()
// Returns whether |dict| has a value assocaited with the |key|.
bool HasEntry(const base::DictionaryValue& dict, const std::string& key) {
  return dict.GetDictionaryWithoutPathExpansion(key, nullptr);
}
#endif

}  // namespace

// static
void MediaDrmStorageImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kMediaDrmStorage);
}

MediaDrmStorageImpl::MediaDrmStorageImpl(
    content::RenderFrameHost* render_frame_host,
    PrefService* pref_service,
    const url::Origin& origin,
    media::mojom::MediaDrmStorageRequest request)
    : render_frame_host_(render_frame_host),
      pref_service_(pref_service),
      origin_(origin),
      origin_string_(origin.Serialize()),
      binding_(this, std::move(request)) {
  DVLOG(1) << __func__ << ": origin = " << origin;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(pref_service_);
  DCHECK(!origin_string_.empty());

  // |this| owns |binding_|, so unretained is safe.
  binding_.set_connection_error_handler(
      base::Bind(&MediaDrmStorageImpl::Close, base::Unretained(this)));
}

MediaDrmStorageImpl::~MediaDrmStorageImpl() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());
}

// TODO(xhwang): Update this function to return an origin ID. If the origin is
// not the same as |origin_|, return an empty origin ID.
void MediaDrmStorageImpl::Initialize(const url::Origin& origin) {
  DVLOG(1) << __func__ << ": origin = " << origin;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);

  initialized_ = true;
}

void MediaDrmStorageImpl::OnProvisioned(const OnProvisionedCallback& callback) {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, kMediaDrmStorage);
  base::DictionaryValue* storage_dict = update.Get();
  DCHECK(storage_dict);

  // The origin string may contain dots. Do not use path expansion.
  DVLOG_IF(1, HasEntry(*storage_dict, origin_string_))
      << __func__ << ": Entry for origin " << origin_string_
      << " already exists and will be cleared";

  storage_dict->SetWithoutPathExpansion(origin_string_,
                                        CreateOriginDictionary());
  callback.Run(true);
}

void MediaDrmStorageImpl::SavePersistentSession(
    const std::string& session_id,
    media::mojom::SessionDataPtr session_data,
    const SavePersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, kMediaDrmStorage);
  base::DictionaryValue* storage_dict = update.Get();
  DCHECK(storage_dict);

  base::DictionaryValue* origin_dict = nullptr;
  // The origin string may contain dots. Do not use path expansion.
  storage_dict->GetDictionaryWithoutPathExpansion(origin_string_, &origin_dict);

  // This could happen if the profile is removed, but the device is still
  // provisioned for the origin. In this case, just create a new entry.
  if (!origin_dict) {

    DVLOG(1) << __func__ << ": Entry for origin " << origin_string_
             << " does not exist; create a new one.";
    storage_dict->SetWithoutPathExpansion(origin_string_,
                                          CreateOriginDictionary());
    storage_dict->GetDictionaryWithoutPathExpansion(origin_string_,
                                                    &origin_dict);
    DCHECK(origin_dict);
  }

  base::DictionaryValue* sessions_dict = nullptr;
  if (!origin_dict->GetDictionary(kSessions, &sessions_dict)) {
    DVLOG(2) << __func__ << ": No session exists; creating a new dict.";
    origin_dict->Set(kSessions, base::MakeUnique<base::DictionaryValue>());
    origin_dict->GetDictionary(kSessions, &sessions_dict);
    DCHECK(sessions_dict);
  }

  DVLOG_IF(1, HasEntry(*sessions_dict, session_id))
      << __func__ << ": Session ID already exists and will be replaced.";

  sessions_dict->SetWithoutPathExpansion(
      session_id, CreateSessionDictionary(session_data->key_set_id,
                                          session_data->mime_type));

  callback.Run(true);
}

void MediaDrmStorageImpl::LoadPersistentSession(
    const std::string& session_id,
    const LoadPersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(nullptr);
    return;
  }

  const base::DictionaryValue* storage_dict =
      pref_service_->GetDictionary(kMediaDrmStorage);

  const base::DictionaryValue* origin_dict = nullptr;
  // The origin string may contain dots. Do not use path expansion.
  storage_dict->GetDictionaryWithoutPathExpansion(origin_string_, &origin_dict);
  if (!origin_dict) {
    DVLOG(1) << __func__
             << ": Failed to save persistent session data; entry for origin "
             << origin_ << " does not exist.";
    callback.Run(nullptr);
    return;
  }

  const base::DictionaryValue* sessions_dict = nullptr;
  if (!origin_dict->GetDictionary(kSessions, &sessions_dict)) {
    DVLOG(2) << __func__ << ": Sessions dictionary does not exist.";
    callback.Run(nullptr);
    return;
  }

  const base::DictionaryValue* session_dict = nullptr;
  if (!sessions_dict->GetDictionaryWithoutPathExpansion(session_id,
                                                        &session_dict)) {
    DVLOG(2) << __func__ << ": Session dictionary does not exist.";
    callback.Run(nullptr);
    return;
  }

  std::vector<uint8_t> key_set_id;
  std::string mime_type;
  if (!GetSessionData(session_dict, &key_set_id, &mime_type)) {
    DVLOG(2) << __func__ << ": Failed to read session data.";
    callback.Run(nullptr);
    return;
  }

  callback.Run(media::mojom::SessionData::New(key_set_id, mime_type));
}

void MediaDrmStorageImpl::RemovePersistentSession(
    const std::string& session_id,
    const RemovePersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, kMediaDrmStorage);
  base::DictionaryValue* storage_dict = update.Get();
  DCHECK(storage_dict);

  base::DictionaryValue* origin_dict = nullptr;
  // The origin string may contain dots. Do not use path expansion.
  storage_dict->GetDictionaryWithoutPathExpansion(origin_string_, &origin_dict);
  if (!origin_dict) {
    DVLOG(1) << __func__ << ": Entry for rigin " << origin_string_
             << " does not exist.";
    callback.Run(true);
    return;
  }

  base::DictionaryValue* sessions_dict = nullptr;
  if (!origin_dict->GetDictionary(kSessions, &sessions_dict)) {
    DVLOG(2) << __func__ << ": Sessions dictionary does not exist.";
    callback.Run(true);
    return;
  }

  sessions_dict->RemoveWithoutPathExpansion(session_id, nullptr);
  callback.Run(true);
}

void MediaDrmStorageImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (render_frame_host == render_frame_host_) {
    DVLOG(1) << __func__ << ": RenderFrame destroyed.";
    Close();
  }
}

void MediaDrmStorageImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (navigation_handle->GetRenderFrameHost() == render_frame_host_) {
    DVLOG(1) << __func__ << ": Close connection on navigation.";
    Close();
  }
}

void MediaDrmStorageImpl::Close() {
  DVLOG(1) << __func__;
  DCHECK(thread_checker_.CalledOnValidThread());

  delete this;
}

}  // namespace cdm
