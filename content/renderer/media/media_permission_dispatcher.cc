// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/gurl.h"

namespace content {

MediaPermissionDispatcher::MediaPermissionDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), next_request_id_(0) {
}

MediaPermissionDispatcher::~MediaPermissionDispatcher() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Fire all pending callbacks with |false|.
  for (auto& request : requests_)
    request.second.Run(false);

  requests_.clear();
}

void MediaPermissionDispatcher::HasPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(PROTECTED_MEDIA_IDENTIFIER, type);

  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        &permission_service_);
  }

  uint32_t request_id = next_request_id_++;
  DCHECK(!requests_.count(request_id));
  requests_[request_id] = permission_status_cb;

  DVLOG(2) << __FUNCTION__ << ": request ID " << request_id;

  permission_service_->HasPermission(
      PERMISSION_NAME_PROTECTED_MEDIA_IDENTIFIER, security_origin.spec(),
      base::Bind(&MediaPermissionDispatcher::OnPermissionStatus,
                 base::Unretained(this), request_id));
}

void MediaPermissionDispatcher::RequestPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(PROTECTED_MEDIA_IDENTIFIER, type);

  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        &permission_service_);
  }

  uint32_t request_id = next_request_id_++;
  DCHECK(!requests_.count(request_id));
  requests_[request_id] = permission_status_cb;

  DVLOG(2) << __FUNCTION__ << ": request ID " << request_id;

  permission_service_->RequestPermission(
      PERMISSION_NAME_PROTECTED_MEDIA_IDENTIFIER, security_origin.spec(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&MediaPermissionDispatcher::OnPermissionStatus,
                 base::Unretained(this), request_id));
}

void MediaPermissionDispatcher::OnPermissionStatus(uint32_t request_id,
                                                   PermissionStatus status) {
  DVLOG(2) << __FUNCTION__ << ": (" << request_id << ", " << status << ")";
  DCHECK(thread_checker_.CalledOnValidThread());

  RequestMap::iterator iter = requests_.find(request_id);
  if (iter == requests_.end()) {
    DVLOG(2) << "Request not found.";
    return;
  }

  PermissionStatusCB permission_status_cb = iter->second;
  requests_.erase(iter);

  permission_status_cb.Run(status == PERMISSION_STATUS_GRANTED);
}

}  // namespace content
