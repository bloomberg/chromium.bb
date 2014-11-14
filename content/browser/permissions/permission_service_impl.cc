// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/permissions/permission_service_impl.h"

#include "content/browser/geolocation/geolocation_provider_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

PermissionType PermissionNameToPermissionType(PermissionName name) {
  switch(name) {
    case PERMISSION_NAME_GEOLOCATION:
      return PERMISSION_GEOLOCATION;
  }

  NOTREACHED();
  return PERMISSION_NUM;
}

} // anonymous namespace

PermissionServiceImpl::PendingRequest::PendingRequest(PermissionType permission,
                                                      const GURL& origin)
    : permission(permission),
      origin(origin) {
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

  PermissionType permission_type = PermissionNameToPermissionType(permission);
  int request_id = pending_requests_.Add(
      new PendingRequest(permission_type, GURL(origin)));

  GetContentClient()->browser()->RequestPermission(
      permission_type,
      context_->web_contents(),
      request_id,
      GURL(origin),
      true, // TODO(mlamouri): should be removed, see http://crbug.com/423770
      base::Bind(&PermissionServiceImpl::OnRequestPermissionResponse,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 request_id));
}

void PermissionServiceImpl::OnRequestPermissionResponse(
    const mojo::Callback<void(PermissionStatus)>& callback,
    int request_id,
    bool allowed) {
  // TODO(mlamouri): we might want a generic way to handle those things.
  if (allowed &&
      pending_requests_.Lookup(request_id)->permission ==
          PERMISSION_GEOLOCATION) {
    GeolocationProviderImpl::GetInstance()->UserDidOptIntoLocationServices();
  }

  pending_requests_.Remove(request_id);

  // TODO(mlamouri): for now, we only get a boolean back, but we would ideally
  // need a ContentSetting, see http://crbug.com/432978
  callback.Run(allowed ? PERMISSION_STATUS_GRANTED : PERMISSION_STATUS_ASK);
}

void PermissionServiceImpl::CancelPendingRequests() {
  DCHECK(context_->web_contents());

  for (RequestsMap::Iterator<PendingRequest> it(&pending_requests_);
       !it.IsAtEnd(); it.Advance()) {
    GetContentClient()->browser()->CancelPermissionRequest(
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
  NOTIMPLEMENTED();
}

}  // namespace content
