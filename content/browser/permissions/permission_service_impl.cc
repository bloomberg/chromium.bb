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
    case PERMISSION_NAME_MIDI:
      return PermissionType::MIDI;
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
    const PermissionStatusCallback& callback)
    : permission(permission),
      origin(origin),
      callback(callback) {
}

PermissionServiceImpl::PendingRequest::~PendingRequest() {
  if (!callback.is_null())
    callback.Run(PERMISSION_STATUS_ASK);
}

PermissionServiceImpl::PendingSubscription::PendingSubscription(
    PermissionType permission,
    const GURL& origin,
    const PermissionStatusCallback& callback)
    : id(-1),
      permission(permission),
      origin(origin),
      callback(callback) {
}

PermissionServiceImpl::PendingSubscription::~PendingSubscription() {
  if (!callback.is_null())
    callback.Run(PERMISSION_STATUS_ASK);
}

PermissionServiceImpl::PermissionServiceImpl(
    PermissionServiceContext* context,
    mojo::InterfaceRequest<PermissionService> request)
    : context_(context),
      binding_(this, request.Pass()),
      weak_factory_(this) {
  binding_.set_connection_error_handler(
      base::Bind(&PermissionServiceImpl::OnConnectionError,
                 base::Unretained(this)));
}

PermissionServiceImpl::~PermissionServiceImpl() {
  DCHECK(pending_requests_.IsEmpty());
}

void PermissionServiceImpl::OnConnectionError() {
  context_->ServiceHadConnectionError(this);
  // After that call, |this| will be deleted.
}

void PermissionServiceImpl::RequestPermission(
    PermissionName permission,
    const mojo::String& origin,
    bool user_gesture,
    const PermissionStatusCallback& callback) {
  // This condition is valid if the call is coming from a ChildThread instead of
  // a RenderFrame. Some consumers of the service run in Workers and some in
  // Frames. In the context of a Worker, it is not possible to show a
  // permission prompt because there is no tab. In the context of a Frame, we
  // can. Even if the call comes from a context where it is not possible to show
  // any UI, we want to still return something relevant so the current
  // permission status is returned.
  if (!context_->render_frame_host()) {
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
      context_->render_frame_host(),
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
  PermissionStatusCallback callback(request->callback);
  request->callback.reset();
  pending_requests_.Remove(request_id);
  callback.Run(status);
}

void PermissionServiceImpl::CancelPendingOperations() {
  DCHECK(context_->render_frame_host());
  DCHECK(context_->GetBrowserContext());

  PermissionManager* permission_manager =
      context_->GetBrowserContext()->GetPermissionManager();
  if (!permission_manager)
    return;

  // Cancel pending requests.
  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    permission_manager->CancelPermissionRequest(
        it.GetCurrentValue()->permission,
        context_->render_frame_host(),
        it.GetCurrentKey(),
        it.GetCurrentValue()->origin);
  }
  pending_requests_.Clear();

  // Cancel pending subscriptions.
  for (SubscriptionsMap::Iterator<PendingSubscription>
          it(&pending_subscriptions_); !it.IsAtEnd(); it.Advance()) {
    it.GetCurrentValue()->callback.Run(GetPermissionStatusFromType(
        it.GetCurrentValue()->permission, it.GetCurrentValue()->origin));
    it.GetCurrentValue()->callback.reset();
    permission_manager->UnsubscribePermissionStatusChange(
        it.GetCurrentValue()->id);
  }
  pending_subscriptions_.Clear();
}

void PermissionServiceImpl::HasPermission(
    PermissionName permission,
    const mojo::String& origin,
    const PermissionStatusCallback& callback) {
  callback.Run(GetPermissionStatusFromName(permission, GURL(origin)));
}

void PermissionServiceImpl::RevokePermission(
    PermissionName permission,
    const mojo::String& origin,
    const PermissionStatusCallback& callback) {
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
    const mojo::String& mojo_origin,
    PermissionStatus last_known_status,
    const PermissionStatusCallback& callback) {
  GURL origin(mojo_origin);
  PermissionStatus current_status =
      GetPermissionStatusFromName(permission, origin);
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

  PermissionType permission_type = PermissionNameToPermissionType(permission);

  // We need to pass the id of PendingSubscription in pending_subscriptions_
  // to the callback but SubscribePermissionStatusChange() will also return an
  // id which is different.
  PendingSubscription* subscription =
      new PendingSubscription(permission_type, origin, callback);
  int pending_subscription_id = pending_subscriptions_.Add(subscription);

  GURL embedding_origin = context_->GetEmbeddingOrigin();
  subscription->id =
      browser_context->GetPermissionManager()->SubscribePermissionStatusChange(
          permission_type,
          origin,
          // If the embedding_origin is empty, we,ll use the |origin| instead.
          embedding_origin.is_empty() ? origin : embedding_origin,
          base::Bind(&PermissionServiceImpl::OnPermissionStatusChanged,
                     weak_factory_.GetWeakPtr(),
                     pending_subscription_id));
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
    int pending_subscription_id,
    PermissionStatus status) {
  PendingSubscription* subscription =
      pending_subscriptions_.Lookup(pending_subscription_id);

  BrowserContext* browser_context = context_->GetBrowserContext();
  DCHECK(browser_context);
  if (browser_context->GetPermissionManager()) {
    browser_context->GetPermissionManager()->UnsubscribePermissionStatusChange(
        subscription->id);
  }

  PermissionStatusCallback callback = subscription->callback;

  subscription->callback.reset();
  pending_subscriptions_.Remove(pending_subscription_id);

  callback.Run(status);
}

}  // namespace content
