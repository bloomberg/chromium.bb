// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"

namespace content {

namespace {

PermissionType PermissionNameToPermissionType(PermissionName name) {
  switch(name) {
    case PERMISSION_NAME_GEOLOCATION:
      return PermissionType::GEOLOCATION;
    case PERMISSION_NAME_NOTIFICATIONS:
      return PermissionType::NOTIFICATIONS;
    case PERMISSION_NAME_PUSH_NOTIFICATIONS:
      return PermissionType::PUSH_MESSAGING;
    case PERMISSION_NAME_MIDI_SYSEX:
      return PermissionType::MIDI_SYSEX;
    case PERMISSION_NAME_PROTECTED_MEDIA_IDENTIFIER:
      return PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  }

  NOTREACHED();
  return PermissionType::NUM;
}

} // anonymous namespace

PermissionServiceImpl::PendingRequest::PendingRequest(
    PermissionType permission,
    const GURL& origin,
    const mojo::Callback<void(PermissionStatus)>& callback)
    : permission(permission),
      origin(origin),
      callback(callback) {
}

PermissionServiceImpl::PendingRequest::~PendingRequest() {
  if (!callback.is_null()) {
    callback.Run(PERMISSION_STATUS_ASK);
  }
}

PermissionServiceImpl::PermissionServiceImpl(PermissionServiceContext* context)
    : context_(context),
      weak_factory_(this) {
}

PermissionServiceImpl::~PermissionServiceImpl() {
}

void PermissionServiceImpl::OnConnectionError() {
  context_->ServiceHadConnectionError(this);
  // After that call, |this| will be deleted.
}

void PermissionServiceImpl::RequestPermission(
    PermissionName permission,
    const mojo::String& origin,
    bool user_gesture,
    const mojo::Callback<void(PermissionStatus)>& callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned.
  if (!context_->web_contents()) {
    // There is no way to show a UI so the call will simply return the current
    // permission.
    HasPermission(permission, origin, callback);
    return;
  }

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager()) {
    callback.Run(content::PERMISSION_STATUS_DENIED);
    return;
  }

  PermissionType permission_type = PermissionNameToPermissionType(permission);
  int request_id = pending_requests_.Add(
      new PendingRequest(permission_type, GURL(origin), callback));

  browser_context->GetPermissionManager()->RequestPermission(
      permission_type,
      context_->web_contents(),
      request_id,
      GURL(origin),
      user_gesture, // TODO(mlamouri): should be removed (crbug.com/423770)
      base::Bind(&PermissionServiceImpl::OnRequestPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 request_id));
}

void PermissionServiceImpl::OnRequestPermissionResponse(
    int request_id,
    PermissionStatus status) {
  PendingRequest* request = pending_requests_.Lookup(request_id);
  mojo::Callback<void(PermissionStatus)> callback(request->callback);
  request->callback.reset();
  pending_requests_.Remove(request_id);
  callback.Run(status);
}

void PermissionServiceImpl::CancelPendingRequests() {
  DCHECK(context_->web_contents());
  DCHECK(context_->GetBrowserContext());

  PermissionManager* permission_manager =
      context_->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager)
    return;

  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    permission_manager->CancelPermissionRequest(
        it.GetCurrentValue()->permission,
        context_->web_contents(),
        it.GetCurrentKey(),
        it.GetCurrentValue()->origin);
  }

  pending_requests_.Clear();
}

void PermissionServiceImpl::HasPermission(
    PermissionName permission,
    const mojo::String& origin,
    const mojo::Callback<void(PermissionStatus)>& callback) {
  DCHECK(context_->GetBrowserContext());

  callback.Run(GetPermissionStatusFromName(permission, GURL(origin)));
}

void PermissionServiceImpl::RevokePermission(
    PermissionName permission,
    const mojo::String& origin,
    const mojo::Callback<void(PermissionStatus)>& callback) {
  GURL origin_url(origin);
  PermissionType permission_type = PermissionNameToPermissionType(permission);
  PermissionStatus status = GetPermissionStatusFromType(permission_type,
                                                        origin_url);

  // Resetting the permission should only be possible if the permission is
  // already granted.
  if (status != PERMISSION_STATUS_GRANTED) {
    callback.Run(status);
    return;
  }

  ResetPermissionStatus(permission_type, origin_url);

  callback.Run(GetPermissionStatusFromType(permission_type, origin_url));
}

void PermissionServiceImpl::GetNextPermissionChange(
    PermissionName permission,
    const mojo::String& origin,
    PermissionStatus last_known_status,
    const mojo::Callback<void(PermissionStatus)>& callback) {
  PermissionStatus current_status =
      GetPermissionStatusFromName(permission, GURL(origin));
  if (current_status != last_known_status) {
    callback.Run(current_status);
    return;
  }

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager()) {
    callback.Run(current_status);
    return;
  }

  int* subscription_id = new int();

  GURL embedding_origin = context_->GetEmbeddingOrigin();
  *subscription_id =
      browser_context->GetPermissionManager()->SubscribePermissionStatusChange(
          PermissionNameToPermissionType(permission),
          GURL(origin),
          // If the embedding_origin is empty, we,ll use the |origin| instead.
          embedding_origin.is_empty() ? GURL(origin) : embedding_origin,
          base::Bind(&PermissionServiceImpl::OnPermissionStatusChanged,
                     weak_factory_.GetWeakPtr(),
                     callback,
                     base::Owned(subscription_id)));
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromName(
    PermissionName permission, const GURL& origin) {
  return GetPermissionStatusFromType(PermissionNameToPermissionType(permission),
                                     origin);
}

PermissionStatus PermissionServiceImpl::GetPermissionStatusFromType(
    PermissionType type, const GURL& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return PERMISSION_STATUS_DENIED;

  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  return browser_context->GetPermissionManager()->GetPermissionStatus(
      type, origin, embedding_origin.is_empty() ? origin : embedding_origin);
}

void PermissionServiceImpl::ResetPermissionStatus(PermissionType type,
                                                  const GURL& origin) {
  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return;

  // If the embedding_origin is empty we'll use |origin| instead.
  GURL embedding_origin = context_->GetEmbeddingOrigin();
  browser_context->GetPermissionManager()->ResetPermission(
      type, origin, embedding_origin.is_empty() ? origin : embedding_origin);
}

void PermissionServiceImpl::OnPermissionStatusChanged(
    const mojo::Callback<void(PermissionStatus)>& callback,
    const int* subscription_id,
    PermissionStatus status) {
  callback.Run(status);

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (!browser_context->GetPermissionManager())
    return;
  browser_context->GetPermissionManager()->UnsubscribePermissionStatusChange(
      *subscription_id);
}

}  // namespace content
