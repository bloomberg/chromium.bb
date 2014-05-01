// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/permission/permission_request_handler.h"

#include "android_webview/native/permission/aw_permission_request.h"
#include "android_webview/native/permission/aw_permission_request_delegate.h"
#include "android_webview/native/permission/permission_request_handler_client.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"

using base::android::ScopedJavaLocalRef;

namespace android_webview {

PermissionRequestHandler::PermissionRequestHandler(
    PermissionRequestHandlerClient* client)
    : client_(client) {
}

PermissionRequestHandler::~PermissionRequestHandler() {
  for (RequestIterator i = requests_.begin(); i != requests_.end(); ++i)
    CancelRequest(i);
}

void PermissionRequestHandler::SendRequest(
    scoped_ptr<AwPermissionRequestDelegate> request) {
  AwPermissionRequest* aw_request = new AwPermissionRequest(request.Pass());
  requests_.push_back(
      base::WeakPtr<AwPermissionRequest>(aw_request->GetWeakPtr()));
  client_->OnPermissionRequest(aw_request);
  PruneRequests();
}

void PermissionRequestHandler::CancelRequest(const GURL& origin,
                                             int64 resources) {
  // The request list might have multiple requests with same origin and
  // resources.
  RequestIterator i = FindRequest(origin, resources);
  while (i != requests_.end()) {
    CancelRequest(i);
    requests_.erase(i);
    i = FindRequest(origin, resources);
  }
}

PermissionRequestHandler::RequestIterator
PermissionRequestHandler::FindRequest(const GURL& origin,
                                      int64 resources) {
  RequestIterator i;
  for (i = requests_.begin(); i != requests_.end(); ++i) {
    if (i->get() && i->get()->GetOrigin() == origin &&
        i->get()->GetResources() == resources) {
      break;
    }
  }
  return i;
}

void PermissionRequestHandler::CancelRequest(RequestIterator i) {
  if (i->get())
    client_->OnPermissionRequestCanceled(i->get());
  // The request's grant()/deny() could be called upon
  // OnPermissionRequestCanceled. Delete AwPermissionRequest if it still
  // exists.
  if (i->get())
    delete i->get();
}

void PermissionRequestHandler::PruneRequests() {
  for (RequestIterator i = requests_.begin(); i != requests_.end();) {
    if (!i->get())
      i = requests_.erase(i);
    else
      ++i;
  }
}

}  // namespace android_webivew
