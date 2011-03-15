// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/resolve_proxy_msg_helper.h"

#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "content/common/child_process_messages.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context.h"

ResolveProxyMsgHelper::ResolveProxyMsgHelper(net::ProxyService* proxy_service)
    : proxy_service_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_(
          this, &ResolveProxyMsgHelper::OnResolveProxyCompleted)),
      proxy_service_override_(proxy_service) {
}

bool ResolveProxyMsgHelper::OnMessageReceived(const IPC::Message& message,
                                              bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ResolveProxyMsgHelper, message, *message_was_ok)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChildProcessHostMsg_ResolveProxy,
                                    OnResolveProxy)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ResolveProxyMsgHelper::OnResolveProxy(const GURL& url,
                                           IPC::Message* reply_msg) {
  // Enqueue the pending request.
  pending_requests_.push_back(PendingRequest(url, reply_msg));

  // If nothing is in progress, start.
  if (pending_requests_.size() == 1)
    StartPendingRequest();
}

void ResolveProxyMsgHelper::OnResolveProxyCompleted(int result) {
  CHECK(!pending_requests_.empty());

  const PendingRequest& completed_req = pending_requests_.front();
  ChildProcessHostMsg_ResolveProxy::WriteReplyParams(
      completed_req.reply_msg, result, proxy_info_.ToPacString());
  Send(completed_req.reply_msg);

  // Clear the current (completed) request.
  pending_requests_.pop_front();
  proxy_service_ = NULL;

  // Start the next request.
  if (!pending_requests_.empty())
    StartPendingRequest();
}

void ResolveProxyMsgHelper::StartPendingRequest() {
  PendingRequest& req = pending_requests_.front();

  // Verify the request wasn't started yet.
  DCHECK(NULL == req.pac_req);
  DCHECK(NULL == proxy_service_);

  // Start the request.
  bool ok = GetProxyService(&proxy_service_);

  if (!ok) {
    // During shutdown, there may be no ProxyService to use, because the
    // default ChromeURLRequestContext has already been NULL-ed out.
    LOG(WARNING) << "Failed getting default URLRequestContext";
    OnResolveProxyCompleted(net::ERR_FAILED);
    return;
  }

  int result = proxy_service_->ResolveProxy(
      req.url, &proxy_info_, &callback_, &req.pac_req, net::BoundNetLog());

  // Completed synchronously.
  if (result != net::ERR_IO_PENDING)
    OnResolveProxyCompleted(result);
}

bool ResolveProxyMsgHelper::GetProxyService(
    scoped_refptr<net::ProxyService>* out) const {
  // Unit-tests specify their own proxy service to use.
  if (proxy_service_override_) {
    *out = proxy_service_override_;
    return true;
  }

  // If there is no default request context (say during shut down).
  URLRequestContextGetter* context_getter =
      Profile::GetDefaultRequestContext();
  if (!context_getter)
    return false;

  // Otherwise use the browser's global proxy service.
  *out = context_getter->GetURLRequestContext()->proxy_service();
  return true;
}

ResolveProxyMsgHelper::~ResolveProxyMsgHelper() {
  // Clear all pending requests.
  if (!pending_requests_.empty()) {
    PendingRequest req = pending_requests_.front();
    proxy_service_->CancelPacRequest(req.pac_req);
  }

  for (PendingRequestList::iterator it = pending_requests_.begin();
       it != pending_requests_.end();
       ++it) {
    delete it->reply_msg;
  }

  proxy_service_ = NULL;
  pending_requests_.clear();
}
