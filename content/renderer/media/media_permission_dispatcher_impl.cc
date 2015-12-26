// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/common/service_registry.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/media/media_permission_dispatcher_proxy.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "url/gurl.h"

namespace {

using Type = media::MediaPermission::Type;

content::PermissionName MediaPermissionTypeToPermissionName(Type type) {
  switch (type) {
    case Type::PROTECTED_MEDIA_IDENTIFIER:
      return content::PERMISSION_NAME_PROTECTED_MEDIA_IDENTIFIER;
    case Type::AUDIO_CAPTURE:
      return content::PERMISSION_NAME_AUDIO_CAPTURE;
    case Type::VIDEO_CAPTURE:
      return content::PERMISSION_NAME_VIDEO_CAPTURE;
  }
  NOTREACHED();
  return content::PERMISSION_NAME_PROTECTED_MEDIA_IDENTIFIER;
}

}  // namespace

namespace content {

MediaPermissionDispatcherImpl::MediaPermissionDispatcherImpl(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {}

MediaPermissionDispatcherImpl::~MediaPermissionDispatcherImpl() {
  DCHECK(thread_checker().CalledOnValidThread());
}

void MediaPermissionDispatcherImpl::HasPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(thread_checker().CalledOnValidThread());

  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  permission_service_->HasPermission(
      MediaPermissionTypeToPermissionName(type), security_origin.spec(),
      base::Bind(&MediaPermissionDispatcherImpl::OnPermissionStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 RegisterCallback(permission_status_cb)));
}

void MediaPermissionDispatcherImpl::RequestPermission(
    Type type,
    const GURL& security_origin,
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(thread_checker().CalledOnValidThread());

  if (!permission_service_.get()) {
    render_frame()->GetServiceRegistry()->ConnectToRemoteService(
        mojo::GetProxy(&permission_service_));
  }

  permission_service_->RequestPermission(
      MediaPermissionTypeToPermissionName(type), security_origin.spec(),
      blink::WebUserGestureIndicator::isProcessingUserGesture(),
      base::Bind(&MediaPermissionDispatcherImpl::OnPermissionStatus,
                 weak_ptr_factory_.GetWeakPtr(),
                 RegisterCallback(permission_status_cb)));
}

scoped_ptr<media::MediaPermission> MediaPermissionDispatcherImpl::CreateProxy(
    scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner) {
  scoped_ptr<media::MediaPermission> media_permission_proxy(
      new MediaPermissionDispatcherProxy(task_runner_, caller_task_runner,
                                         weak_ptr_factory_.GetWeakPtr()));
  return media_permission_proxy;
}

void MediaPermissionDispatcherImpl::OnPermissionStatus(
    uint32_t request_id,
    PermissionStatus status) {
  DCHECK(thread_checker().CalledOnValidThread());
  DeliverResult(request_id, status == PERMISSION_STATUS_GRANTED);
}

}  // namespace content
