// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_URL_LOADER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_URL_LOADER_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/html_viewer/mock_web_blob_registry_impl.h"
#include "mojo/message_pump/handle_watcher.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebReferrerPolicy.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"

namespace mojo {
class URLLoaderFactory;
}

namespace html_viewer {

// The concrete type of WebURLRequest::ExtraData.
class WebURLRequestExtraData : public blink::WebURLRequest::ExtraData {
 public:
  WebURLRequestExtraData();
  virtual ~WebURLRequestExtraData();

  mojo::URLResponsePtr synthetic_response;
};

class WebURLLoaderImpl : public blink::WebURLLoader {
 public:
  explicit WebURLLoaderImpl(mojo::URLLoaderFactory* url_loader_factory,
                            MockWebBlobRegistryImpl* web_blob_registry);

 private:
  virtual ~WebURLLoaderImpl();

  // blink::WebURLLoader methods:
  virtual void loadSynchronously(const blink::WebURLRequest& request,
                                 blink::WebURLResponse& response,
                                 blink::WebURLError& error,
                                 blink::WebData& data);
  virtual void loadAsynchronously(const blink::WebURLRequest&,
                                  blink::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool defers_loading);
  virtual void setLoadingTaskRunner(blink::WebTaskRunner* web_task_runner);

  void OnReceivedResponse(const blink::WebURLRequest& request,
                          mojo::URLResponsePtr response);
  void OnReceivedError(mojo::URLResponsePtr response);
  void OnReceivedRedirect(const blink::WebURLRequest& request,
                          mojo::URLResponsePtr response);
  void OnReceiveWebBlobData(
      const blink::WebURLRequest& request,
      const blink::WebVector<blink::WebBlobData::Item*>& items);
  void ReadMore();
  void WaitToReadMore();
  void OnResponseBodyStreamReady(MojoResult result);

  blink::WebURLLoaderClient* client_;
  MockWebBlobRegistryImpl* web_blob_registry_;
  GURL url_;
  blink::WebReferrerPolicy referrer_policy_;
  mojo::URLLoaderPtr url_loader_;
  mojo::ScopedDataPipeConsumerHandle response_body_stream_;
  mojo::common::HandleWatcher handle_watcher_;

  base::WeakPtrFactory<WebURLLoaderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_URL_LOADER_IMPL_H_
