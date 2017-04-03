// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/media_drm_storage_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
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
      binding_(this, std::move(request)) {
  DVLOG(1) << __func__ << ": origin = " << origin;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(pref_service_);

  // |this| owns |binding_|, so unretained is safe.
  binding_.set_connection_error_handler(
      base::Bind(&MediaDrmStorageImpl::Close, base::Unretained(this)));
}

MediaDrmStorageImpl::~MediaDrmStorageImpl() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

// TODO(xhwang): Update this function to return an origin ID. If the origin is
// not the same as |origin_|, return an empty origin ID.
void MediaDrmStorageImpl::Initialize(const url::Origin& origin) {
  DVLOG(1) << __func__ << ": origin = " << origin;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!initialized_);

  initialized_ = true;
}

void MediaDrmStorageImpl::OnProvisioned(const OnProvisionedCallback& callback) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(false);
}

void MediaDrmStorageImpl::SavePersistentSession(
    const std::string& session_id,
    media::mojom::SessionDataPtr session_data,
    const SavePersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(false);
}

void MediaDrmStorageImpl::LoadPersistentSession(
    const std::string& session_id,
    const LoadPersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(nullptr);
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(nullptr);
}

void MediaDrmStorageImpl::RemovePersistentSession(
    const std::string& session_id,
    const RemovePersistentSessionCallback& callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    callback.Run(false);
    return;
  }

  NOTIMPLEMENTED();
  callback.Run(false);
}

void MediaDrmStorageImpl::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  if (render_frame_host == render_frame_host_) {
    DVLOG(1) << __func__ << ": RenderFrame destroyed.";
    Close();
  }
}

void MediaDrmStorageImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetRenderFrameHost() == render_frame_host_) {
    DVLOG(1) << __func__ << ": Close connection on navigation.";
    Close();
  }
}

void MediaDrmStorageImpl::Close() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  delete this;
}

}  // namespace cdm
