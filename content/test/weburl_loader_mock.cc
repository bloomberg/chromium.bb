// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/weburl_loader_mock.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/web_url_loader_impl.h"
#include "content/test/weburl_loader_mock_factory.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebUnitTestSupport.h"

WebURLLoaderMock::WebURLLoaderMock(WebURLLoaderMockFactory* factory,
                                   blink::WebURLLoader* default_loader)
    : factory_(factory),
      client_(NULL),
      default_loader_(default_loader),
      using_default_loader_(false),
      is_deferred_(false),
      weak_factory_(this) {
}

WebURLLoaderMock::~WebURLLoaderMock() {
}

void WebURLLoaderMock::ServeAsynchronousRequest(
    blink::WebURLLoaderTestDelegate* delegate,
    const blink::WebURLResponse& response,
    const blink::WebData& data,
    const blink::WebURLError& error) {
  DCHECK(!using_default_loader_);
  if (!client_)
    return;

  // If no delegate is provided then create an empty one. The default behavior
  // will just proxy to the client.
  scoped_ptr<blink::WebURLLoaderTestDelegate> defaultDelegate;
  if (!delegate) {
    defaultDelegate.reset(new blink::WebURLLoaderTestDelegate());
    delegate = defaultDelegate.get();
  }

  // didReceiveResponse() and didReceiveData() might end up getting ::cancel()
  // to be called which will make the ResourceLoader to delete |this|.
  base::WeakPtr<WebURLLoaderMock> self(weak_factory_.GetWeakPtr());

  delegate->didReceiveResponse(client_, this, response);
  if (!self)
    return;

  if (error.reason) {
    delegate->didFail(client_, this, error);
    return;
  }
  delegate->didReceiveData(client_, this, data.data(), data.size(),
                             data.size());
  if (!self)
    return;

  delegate->didFinishLoading(client_, this, 0, data.size());
}

blink::WebURLRequest WebURLLoaderMock::ServeRedirect(
    const blink::WebURLRequest& request,
    const blink::WebURLResponse& redirectResponse) {
  GURL redirectURL(redirectResponse.httpHeaderField("Location"));

  net::RedirectInfo redirectInfo;
  redirectInfo.new_method = request.httpMethod().utf8();
  redirectInfo.new_url = redirectURL;
  redirectInfo.new_first_party_for_cookies = redirectURL;

  blink::WebURLRequest newRequest;
  newRequest.initialize();
  content::WebURLLoaderImpl::PopulateURLRequestForRedirect(
      request,
      redirectInfo,
      request.referrerPolicy(),
      request.skipServiceWorker(),
      &newRequest);

  base::WeakPtr<WebURLLoaderMock> self(weak_factory_.GetWeakPtr());

  client_->willFollowRedirect(this, newRequest, redirectResponse);

  // |this| might be deleted in willFollowRedirect().
  if (!self)
    return newRequest;

  if (redirectURL != GURL(newRequest.url())) {
    // Only follow the redirect if WebKit left the URL unmodified.
    // We assume that WebKit only changes the URL to suppress a redirect, and we
    // assume that it does so by setting it to be invalid.
    DCHECK(!newRequest.url().isValid());
    cancel();
  }

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

void WebURLLoaderMock::setLoadingTaskRunner(blink::WebTaskRunner*) {
  // In principle this is NOTIMPLEMENTED(), but if we put that here it floods
  // the console during webkit unit tests, so we leave the function empty.
}
