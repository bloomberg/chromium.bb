// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "content/public/browser/render_frame_host.h"

using blink::mojom::PermissionDescriptorPtr;
using blink::mojom::PermissionName;
using blink::mojom::PermissionObserverPtr;
using blink::mojom::PermissionStatus;

namespace content {

namespace {

PermissionType PermissionDescriptorToPermissionType(
    const PermissionDescriptorPtr& descriptor) {
  switch (descriptor->name) {
    case PermissionName::GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PermissionName::NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PermissionName::MIDI: {
      if (descriptor->extension && descriptor->extension->is_midi() &&
          descriptor->extension->get_midi()->sysex) {
        return PermissionType::MIDI_SYSEX;
      }
      return PermissionType::MIDI;
    }
    case PermissionName::PROTECTED_MEDIA_IDENTIFIER:
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
    case PermissionName::DURABLE_STORAGE:
      return PermissionType::DURABLE_STORAGE;
    case PermissionName::AUDIO_CAPTURE:
      return PermissionType::AUDIO_CAPTURE;
    case PermissionName::VIDEO_CAPTURE:
      return PermissionType::VIDEO_CAPTURE;
    case PermissionName::BACKGROUND_SYNC:
      return PermissionType::BACKGROUND_SYNC;
    case PermissionName::SENSORS:
      return PermissionType::SENSORS;
    case PermissionName::ACCESSIBILITY_EVENTS:
      return PermissionType::ACCESSIBILITY_EVENTS;
    case PermissionName::CLIPBOARD_READ:
      return PermissionType::CLIPBOARD_READ;
    case PermissionName::CLIPBOARD_WRITE:
      return PermissionType::CLIPBOARD_WRITE;
    case PermissionName::PAYMENT_HANDLER:
      return PermissionType::PAYMENT_HANDLER;
  }

  NOTREACHED();
  return PermissionType::NUM;
}

// This function allows the usage of the the multiple request map with single
// requests.
void PermissionRequestResponseCallbackWrapper(
    base::OnceCallback<void(PermissionStatus)> callback,
    const std::vector<PermissionStatus>& vector) {
  DCHECK_EQ(vector.size(), 1ul);
  std::move(callback).Run(vector[0]);
}

}  // anonymous namespace

class PermissionServiceImpl::PendingRequest {
 public:
  PendingRequest(std::vector<PermissionType> types,
                 RequestPermissionsCallback callback)
      : callback_(std::move(callback)), request_size_(types.size()) {}

  ~PendingRequest() {
    if (callback_.is_null())
      return;

    std::move(callback_).Run(
        std::vector<PermissionStatus>(request_size_, PermissionStatus::DENIED));
  }

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  void RunCallback(const std::vector<PermissionStatus>& results) {
    std::move(callback_).Run(results);
  }

 private:
  // Request ID received from the PermissionManager.
  int id_;
  RequestPermissionsCallback callback_;
  size_t request_size_;
};

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context,
                                             const url::Origin& origin)
    : context_(context), origin_(origin), weak_factory_(this) {}

PermissionServiceImpl::~PermissionServiceImpl() {}

void PermissionServiceImpl::RequestPermission(
    PermissionDescriptorPtr permission,
    bool user_gesture,
    PermissionStatusCallback callback) {
  std::vector<PermissionDescriptorPtr> permissions;
  permissions.push_back(std::move(permission));
  RequestPermissions(std::move(permissions), user_gesture,
                     base::BindOnce(&PermissionRequestResponseCallbackWrapper,
                                    std::move(callback)));
}

void PermissionServiceImpl::RequestPermissions(
    std::vector<PermissionDescriptorPtr> permissions,
    bool user_gesture,
    RequestPermissionsCallback callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned for each permission.
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  if (!context_->render_frame_host() ||
      !browser_context->GetPermissionManager()) {
    std::vector<PermissionStatus> result(permissions.size());
    for (size_t i = 0; i < permissions.size(); ++i)
      result[i] = GetPermissionStatus(permissions[i]);
    std::move(callback).Run(result);
    return;
  }

  std::vector<PermissionType> types(permissions.size());
  for (size_t i = 0; i < types.size(); ++i)
    types[i] = PermissionDescriptorToPermissionType(permissions[i]);

  std::unique_ptr<PendingRequest> pending_request =
      std::make_unique<PendingRequest>(types, std::move(callback));

  int pending_request_id = pending_requests_.Add(std::move(pending_request));
  int id = browser_context->GetPermissionManager()->RequestPermissions(
      types, context_->render_frame_host(), origin_.GetURL(), user_gesture,
      base::Bind(&PermissionServiceImpl::OnRequestPermissionsResponse,
                 weak_factory_.GetWeakPtr(), pending_request_id));

  // Check if the request still exists. It may have been removed by the
  // the response callback.
  PendingRequest* in_progress_request =
      pending_requests_.Lookup(pending_request_id);
  if (!in_progress_request)
    return;
  in_progress_request->set_id(id);
}

void PermissionServiceImpl::OnRequestPermissionsResponse(
    int pending_request_id,
    const std::vector<PermissionStatus>& results) {
  PendingRequest* request = pending_requests_.Lookup(pending_request_id);
  request->RunCallback(results);
  pending_requests_.Remove(pending_request_id);
}

void PermissionServiceImpl::HasPermission(PermissionDescriptorPtr permission,
                                          PermissionStatusCallback callback) {
  std::move(callback).Run(GetPermissionStatus(permission));
}

void PermissionServiceImpl::RevokePermission(
    PermissionDescriptorPtr permission,
    PermissionStatusCallback callback) {
  PermissionType permission_type =
      PermissionDescriptorToPermissionType(permission);
  PermissionStatus status = GetPermissionStatusFromType(permission_type);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PermissionStatus::GRANTED) {
    std::move(callback).Run(status);
    return;
  }

  ResetPermissionStatus(permission_type);

  std::move(callback).Run(GetPermissionStatusFromType(permission_type));
}

void PermissionServiceImpl::AddPermissionObserver(
    PermissionDescriptorPtr permission,
    PermissionStatus last_known_status,
    PermissionObserverPtr observer) {
  PermissionStatus current_status = GetPermissionStatus(permission);
  if (current_status != last_known_status) {
    observer->OnPermissionStatusChange(current_status);
    last_known_status = current_status;
  }

  context_->CreateSubscription(PermissionDescriptorToPermissionType(permission),
                               origin_, std::move(observer));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatus(
    const PermissionDescriptorPtr& permission) {
  return GetPermissionStatusFromType(
      PermissionDescriptorToPermissionType(permission));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return PermissionStatus::DENIED;

  if (!browser_context->GetPermissionManager())
    return PermissionStatus::DENIED;

  GURL requesting_origin(origin_.GetURL());
  if (context_->render_frame_host()) {
    return browser_context->GetPermissionManager()->GetPermissionStatusForFrame(
        type, context_->render_frame_host(), requesting_origin);
  }

  DCHECK(context_->GetEmbeddingOrigin().is_empty());
  return browser_context->GetPermissionManager()->GetPermissionStatus(
      type, requesting_origin, requesting_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  if (!browser_context)
    return;

  if (!browser_context->GetPermissionManager())
    return;

  GURL requesting_origin(origin_.GetURL());
  // If the embedding_origin is empty we'll use |origin_| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  browser_context->GetPermissionManager()->ResetPermission(
      type, requesting_origin,
      embedding_origin.is_empty() ? requesting_origin : embedding_origin);
}

}  // namespace content
