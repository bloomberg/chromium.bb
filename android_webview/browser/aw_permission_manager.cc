// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_permission_manager.h"

#include <string>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

using blink::mojom::PermissionStatus;
using content::PermissionType;

namespace android_webview {

class LastRequestResultCache {
 public:
  LastRequestResultCache() = default;

  void SetResult(PermissionType permission,
                 const GURL& requesting_origin,
                 const GURL& embedding_origin,
                 PermissionStatus status) {
    DCHECK(status == PermissionStatus::GRANTED ||
           status == PermissionStatus::DENIED);

    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      DLOG(WARNING) << "Not caching result because of empty origin.";
      return;
    }

    if (!requesting_origin.is_valid()) {
      NOTREACHED() << requesting_origin.possibly_invalid_spec();
      return;
    }
    if (!embedding_origin.is_valid()) {
      NOTREACHED() << embedding_origin.possibly_invalid_spec();
      return;
    }

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    if (key.empty()) {
      NOTREACHED();
      // Never store an empty key because it could inadvertently be used for
      // another combination.
      return;
    }
    pmi_result_cache_[key] = status;
  }

  PermissionStatus GetResult(PermissionType permission,
                             const GURL& requesting_origin,
                             const GURL& embedding_origin) const {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return PermissionStatus::ASK;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();

    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      NOTREACHED() << "Results are only cached for PROTECTED_MEDIA_IDENTIFIER";
      return PermissionStatus::ASK;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    StatusMap::const_iterator it = pmi_result_cache_.find(key);
    if (it == pmi_result_cache_.end()) {
      DLOG(WARNING) << "GetResult() called for uncached origins: " << key;
      return PermissionStatus::ASK;
    }

    DCHECK(!key.empty());
    return it->second;
  }

  void ClearResult(PermissionType permission,
                   const GURL& requesting_origin,
                   const GURL& embedding_origin) {
    // TODO(ddorwin): We should be denying empty origins at a higher level.
    if (requesting_origin.is_empty() || embedding_origin.is_empty()) {
      return;
    }

    DCHECK(requesting_origin.is_valid())
        << requesting_origin.possibly_invalid_spec();
    DCHECK(embedding_origin.is_valid())
        << embedding_origin.possibly_invalid_spec();


    if (permission != PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
      // Other permissions are not cached, so nothing to clear.
      return;
    }

    std::string key = GetCacheKey(requesting_origin, embedding_origin);
    pmi_result_cache_.erase(key);
  }

 private:
  // Returns a concatenation of the origins to be used as the index.
  // Returns the empty string if either origin is invalid or empty.
  static std::string GetCacheKey(const GURL& requesting_origin,
                                 const GURL& embedding_origin) {
    const std::string& requesting = requesting_origin.spec();
    const std::string& embedding = embedding_origin.spec();
    if (requesting.empty() || embedding.empty())
      return std::string();
    return requesting + "," + embedding;
  }

  using StatusMap = base::hash_map<std::string, PermissionStatus>;
  StatusMap pmi_result_cache_;

  DISALLOW_COPY_AND_ASSIGN(LastRequestResultCache);
};

struct AwPermissionManager::PendingRequest {
 public:
  PendingRequest(PermissionType permission,
                 GURL requesting_origin,
                 GURL embedding_origin,
                 content::RenderFrameHost* render_frame_host,
                 const base::Callback<void(PermissionStatus)>& callback)
    : permission(permission),
      requesting_origin(requesting_origin),
      embedding_origin(embedding_origin),
      render_process_id(render_frame_host->GetProcess()->GetID()),
      render_frame_id(render_frame_host->GetRoutingID()),
      callback(callback) {
  }

  ~PendingRequest() = default;

  PermissionType permission;
  GURL requesting_origin;
  GURL embedding_origin;
  int render_process_id;
  int render_frame_id;
  base::Callback<void(PermissionStatus)> callback;
};

AwPermissionManager::AwPermissionManager()
  : content::PermissionManager(),
    result_cache_(new LastRequestResultCache),
    weak_ptr_factory_(this) {
}

AwPermissionManager::~AwPermissionManager() {
}

int AwPermissionManager::RequestPermission(
    PermissionType permission,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  int render_process_id = render_frame_host->GetProcess()->GetID();
  int render_frame_id = render_frame_host->GetRoutingID();
  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(render_process_id,
                                                 render_frame_id);
  if (!delegate) {
    DVLOG(0) << "Dropping permission request for "
             << static_cast<int>(permission);
    callback.Run(PermissionStatus::DENIED);
    return kNoPendingOperation;
  }

  // Do not delegate any requests which are already pending.
  bool should_delegate_request = true;
  for (PendingRequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    if (permission == it.GetCurrentValue()->permission) {
      should_delegate_request = false;
      break;
    }
  }

  const GURL& embedding_origin =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL().GetOrigin();

  int request_id = kNoPendingOperation;
  switch (permission) {
    case PermissionType::GEOLOCATION:
      request_id = pending_requests_.Add(new PendingRequest(
          permission, requesting_origin,
          embedding_origin, render_frame_host,
          callback));
      if (should_delegate_request) {
        delegate->RequestGeolocationPermission(
            requesting_origin,
            base::Bind(&OnRequestResponse,
                       weak_ptr_factory_.GetWeakPtr(), request_id,
                       callback));
      }
      break;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      request_id = pending_requests_.Add(new PendingRequest(
          permission, requesting_origin,
          embedding_origin, render_frame_host,
          callback));
      if (should_delegate_request) {
        delegate->RequestProtectedMediaIdentifierPermission(
            requesting_origin,
            base::Bind(&OnRequestResponse,
                       weak_ptr_factory_.GetWeakPtr(), request_id,
                       callback));
      }
      break;
    case PermissionType::MIDI_SYSEX:
      request_id = pending_requests_.Add(new PendingRequest(
          permission, requesting_origin,
          embedding_origin, render_frame_host,
          callback));
      if (should_delegate_request) {
        delegate->RequestMIDISysexPermission(
            requesting_origin,
            base::Bind(&OnRequestResponse,
                       weak_ptr_factory_.GetWeakPtr(), request_id,
                       callback));
      }
      break;
    case PermissionType::AUDIO_CAPTURE:
    case PermissionType::VIDEO_CAPTURE:
    case PermissionType::NOTIFICATIONS:
    case PermissionType::PUSH_MESSAGING:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::BACKGROUND_SYNC:
      NOTIMPLEMENTED() << "RequestPermission is not implemented for "
                       << static_cast<int>(permission);
      callback.Run(PermissionStatus::DENIED);
      break;
    case PermissionType::MIDI:
      callback.Run(PermissionStatus::GRANTED);
      break;
    case PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      callback.Run(PermissionStatus::DENIED);
      break;
  }
  return request_id;
}

int AwPermissionManager::RequestPermissions(
    const std::vector<PermissionType>& permissions,
    content::RenderFrameHost* render_frame_host,
    const GURL& requesting_origin,
    const base::Callback<void(
        const std::vector<PermissionStatus>&)>& callback) {
  NOTIMPLEMENTED() << "RequestPermissions has not been implemented in WebView";

  std::vector<PermissionStatus> result(permissions.size());
  const GURL& embedding_origin =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetLastCommittedURL().GetOrigin();

  for (PermissionType type : permissions) {
    result.push_back(GetPermissionStatus(
        type, requesting_origin, embedding_origin));
  }

  callback.Run(result);
  return kNoPendingOperation;
}

// static
void AwPermissionManager::OnRequestResponse(
    const base::WeakPtr<AwPermissionManager>& manager,
    int request_id,
    const base::Callback<void(PermissionStatus)>& callback,
    bool allowed) {
  PermissionStatus status =
      allowed ? PermissionStatus::GRANTED : PermissionStatus::DENIED;
  if (manager.get()) {
    PendingRequest* pending_request =
        manager->pending_requests_.Lookup(request_id);

    for (PendingRequestsMap::Iterator<PendingRequest> it(
            &manager->pending_requests_);
         !it.IsAtEnd(); it.Advance()) {
      if (pending_request->permission == it.GetCurrentValue()->permission &&
          it.GetCurrentKey() != request_id) {
        it.GetCurrentValue()->callback.Run(status);
        manager->pending_requests_.Remove(it.GetCurrentKey());
      }
    }

    manager->result_cache_->SetResult(
        pending_request->permission,
        pending_request->requesting_origin,
        pending_request->embedding_origin,
        status);
    manager->pending_requests_.Remove(request_id);
  }
  callback.Run(status);
}

void AwPermissionManager::CancelPermissionRequest(int request_id) {
  PendingRequest* pending_request = pending_requests_.Lookup(request_id);
  if (!pending_request)
    return;

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(pending_request->render_process_id,
          pending_request->render_frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  DCHECK(web_contents);

  // The caller is canceling (presumably) the most recent request. Assuming the
  // request did not complete, the user did not respond to the requset.
  // Thus, assume we do not know the result.
  const GURL& embedding_origin = web_contents
          ->GetLastCommittedURL().GetOrigin();
  result_cache_->ClearResult(
      pending_request->permission,
      pending_request->requesting_origin,
      embedding_origin);

  AwBrowserPermissionRequestDelegate* delegate =
      AwBrowserPermissionRequestDelegate::FromID(
          pending_request->render_process_id,
          pending_request->render_frame_id);
  if (!delegate) {
    pending_requests_.Remove(request_id);
    return;
  }

  switch (pending_request->permission) {
    case PermissionType::GEOLOCATION:
      delegate->CancelGeolocationPermissionRequests(
          pending_request->requesting_origin);
      break;
    case PermissionType::PROTECTED_MEDIA_IDENTIFIER:
      delegate->CancelProtectedMediaIdentifierPermissionRequests(
          pending_request->requesting_origin);
      break;
    case PermissionType::MIDI_SYSEX:
      delegate->CancelMIDISysexPermissionRequests(
          pending_request->requesting_origin);
      break;
    case PermissionType::NOTIFICATIONS:
    case PermissionType::PUSH_MESSAGING:
    case PermissionType::DURABLE_STORAGE:
    case PermissionType::AUDIO_CAPTURE:
    case PermissionType::VIDEO_CAPTURE:
    case PermissionType::BACKGROUND_SYNC:
      NOTIMPLEMENTED() << "CancelPermission not implemented for "
                       << static_cast<int>(pending_request->permission);
      break;
    case PermissionType::MIDI:
      // There is nothing to cancel so this is simply ignored.
      break;
    case PermissionType::NUM:
      NOTREACHED() << "PermissionType::NUM was not expected here.";
      break;
  }

  pending_requests_.Remove(request_id);
}

void AwPermissionManager::ResetPermission(PermissionType permission,
                                          const GURL& requesting_origin,
                                          const GURL& embedding_origin) {
  result_cache_->ClearResult(permission, requesting_origin, embedding_origin);
}

PermissionStatus AwPermissionManager::GetPermissionStatus(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  // Method is called outside the Permissions API only for this permission.
  if (permission == PermissionType::PROTECTED_MEDIA_IDENTIFIER) {
    return result_cache_->GetResult(permission, requesting_origin,
                                    embedding_origin);
  } else if (permission == PermissionType::MIDI) {
    return PermissionStatus::GRANTED;
  }

  return PermissionStatus::DENIED;
}

void AwPermissionManager::RegisterPermissionUsage(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
}

int AwPermissionManager::SubscribePermissionStatusChange(
    PermissionType permission,
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  return kNoPendingOperation;
}

void AwPermissionManager::UnsubscribePermissionStatusChange(
    int subscription_id) {
}

}  // namespace android_webview
