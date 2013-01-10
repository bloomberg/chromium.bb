// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_data_request_manager.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/webdata/web_data_service.h"

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequest implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequest::WebDataRequest(WebDataService* service,
                               WebDataServiceConsumer* consumer,
                               WebDataRequestManager* manager)
    : service_(service),
      cancelled_(false),
      consumer_(consumer),
      result_(NULL) {
  handle_ = manager->GetNextRequestHandle();
  message_loop_ = MessageLoop::current();
  manager->RegisterRequest(this);
}

WebDataRequest::~WebDataRequest() {
  delete result_;
}

WebDataService::Handle WebDataRequest::GetHandle() const {
  return handle_;
}

WebDataServiceConsumer* WebDataRequest::GetConsumer() const {
  return consumer_;
}

bool WebDataRequest::IsCancelled() const {
  base::AutoLock l(cancel_lock_);
  return cancelled_;
}

void WebDataRequest::Cancel() {
  base::AutoLock l(cancel_lock_);
  cancelled_ = true;
  consumer_ = NULL;
}

void WebDataRequest::SetResult(WDTypedResult* r) {
  result_ = r;
}

const WDTypedResult* WebDataRequest::GetResult() const {
  return result_;
}

void WebDataRequest::RequestComplete() {
  message_loop_->PostTask(FROM_HERE, Bind(&WebDataService::RequestCompleted,
                                          service_.get(), handle_));
}

////////////////////////////////////////////////////////////////////////////////
//
// WebDataRequestManager implementation.
//
////////////////////////////////////////////////////////////////////////////////

WebDataRequestManager::WebDataRequestManager()
    : next_request_handle_(1) {
}

WebDataRequestManager::~WebDataRequestManager() {
}

void WebDataRequestManager::RegisterRequest(WebDataRequest* request) {
  base::AutoLock l(pending_lock_);
  pending_requests_[request->GetHandle()] = request;
}

int WebDataRequestManager::GetNextRequestHandle() {
  base::AutoLock l(pending_lock_);
  return ++next_request_handle_;
}

void WebDataRequestManager::CancelRequest(WebDataServiceBase::Handle h) {
  base::AutoLock l(pending_lock_);
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Canceling a nonexistent web data service request";
    return;
  }
  i->second->Cancel();
}

void WebDataRequestManager::RequestCompleted(WebDataServiceBase::Handle h) {
  pending_lock_.Acquire();
  RequestMap::iterator i = pending_requests_.find(h);
  if (i == pending_requests_.end()) {
    NOTREACHED() << "Request completed called for an unknown request";
    pending_lock_.Release();
    return;
  }

  // Take ownership of the request object and remove it from the map.
  scoped_ptr<WebDataRequest> request(i->second);
  pending_requests_.erase(i);
  pending_lock_.Release();

  // Notify the consumer if needed.
  WebDataServiceConsumer* consumer = request->GetConsumer();
  if (!request->IsCancelled() && consumer) {
    consumer->OnWebDataServiceRequestDone(request->GetHandle(),
                                          request->GetResult());
  } else {
    // Nobody is taken ownership of the result, either because it is cancelled
    // or there is no consumer. Destroy results that require special handling.
    WDTypedResult const *result = request->GetResult();
    if (result) {
      result->Destroy();
    }
  }
}
