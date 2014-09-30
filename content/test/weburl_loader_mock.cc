// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/weburl_loader_mock.h"

#include "base/logging.h"
#include "content/test/weburl_loader_mock_factory.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"

WebURLLoaderMock::WebURLLoaderMock(WebURLLoaderMockFactory* factory,
                                   blink::WebURLLoader* default_loader)
    : factory_(factory),
      client_(NULL),
      default_loader_(default_loader),
      using_default_loader_(false),
      is_deferred_(false),
      this_deleted_(NULL) {
}

WebURLLoaderMock::~WebURLLoaderMock() {
  // When |this_deleted_| is not null, there is someone interested to know if
  // |this| got deleted. We notify them by setting the pointed value to true.
  if (this_deleted_)
    *this_deleted_ = true;
}

void WebURLLoaderMock::ServeAsynchronousRequest(
    const blink::WebURLResponse& response,
    const blink::WebData& data,
    const blink::WebURLError& error) {
  DCHECK(!using_default_loader_);
  if (!client_)
    return;

  bool this_deleted = false;
  this_deleted_ = &this_deleted;
  client_->didReceiveResponse(this, response);

  // didReceiveResponse might end up getting ::cancel() to be called which will
  // make the ResourceLoader to delete |this|. If that happens, |this_deleted|,
  // created on the stack, will be set to true.
  if (this_deleted)
    return;
  this_deleted_ = NULL;

  if (error.reason) {
    client_->didFail(this, error);
    return;
  }
  client_->didReceiveData(this, data.data(), data.size(), data.size());
  client_->didFinishLoading(this, 0, data.size());
}

blink::WebURLRequest WebURLLoaderMock::ServeRedirect(
    const blink::WebURLResponse& redirectResponse) {
  blink::WebURLRequest newRequest;
  newRequest.initialize();
  newRequest.setRequestContext(blink::WebURLRequest::RequestContextInternal);
  GURL redirectURL(redirectResponse.httpHeaderField("Location"));
  newRequest.setURL(redirectURL);
  client_->willSendRequest(this, newRequest, redirectResponse);
  return newRequest;
}

void WebURLLoaderMock::loadSynchronously(const blink::WebURLRequest& request,
                                         blink::WebURLResponse& response,
                                         blink::WebURLError& error,
                                         blink::WebData& data) {
  if (factory_->IsMockedURL(request.url())) {
    factory_->LoadSynchronously(request, &response, &error, &data);
    return;
  }
  DCHECK(static_cast<const GURL&>(request.url()).SchemeIs("data"))
      << "loadSynchronously shouldn't be falling back: "
      << request.url().spec().data();
  using_default_loader_ = true;
  default_loader_->loadSynchronously(request, response, error, data);
}

void WebURLLoaderMock::loadAsynchronously(const blink::WebURLRequest& request,
                                          blink::WebURLLoaderClient* client) {
  if (factory_->IsMockedURL(request.url())) {
    client_ = client;
    factory_->LoadAsynchronouly(request, this);
    return;
  }
  DCHECK(static_cast<const GURL&>(request.url()).SchemeIs("data"))
      << "loadAsynchronously shouldn't be falling back: "
      << request.url().spec().data();
  using_default_loader_ = true;
  default_loader_->loadAsynchronously(request, client);
}

void WebURLLoaderMock::cancel() {
  if (using_default_loader_) {
    default_loader_->cancel();
    return;
  }
  client_ = NULL;
  factory_->CancelLoad(this);
}

void WebURLLoaderMock::setDefersLoading(bool deferred) {
  is_deferred_ = deferred;
  if (using_default_loader_) {
    default_loader_->setDefersLoading(deferred);
    return;
  }
  NOTIMPLEMENTED();
}
