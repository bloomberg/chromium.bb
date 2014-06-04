// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_WEBURL_LOADER_MOCK_H_
#define CONTENT_TEST_WEBURL_LOADER_MOCK_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace blink {
class WebData;
struct WebURLError;
class WebURLLoaderClient;
class WebURLRequest;
class WebURLResponse;
}

class WebURLLoaderMockFactory;

// A simple class for mocking WebURLLoader.
// If the WebURLLoaderMockFactory it is associated with has been configured to
// mock the request it gets, it serves the mocked resource.  Otherwise it just
// forwards it to the default loader.
class WebURLLoaderMock : public blink::WebURLLoader {
 public:
  // This object becomes the owner of |default_loader|.
  WebURLLoaderMock(WebURLLoaderMockFactory* factory,
                   blink::WebURLLoader* default_loader);
  virtual ~WebURLLoaderMock();

  // Simulates the asynchronous request being served.
  void ServeAsynchronousRequest(const blink::WebURLResponse& response,
                                const blink::WebData& data,
                                const blink::WebURLError& error);

  // Simulates the redirect being served.
  blink::WebURLRequest ServeRedirect(
      const blink::WebURLResponse& redirectResponse);

  // WebURLLoader methods:
  virtual void loadSynchronously(const blink::WebURLRequest& request,
                                 blink::WebURLResponse& response,
                                 blink::WebURLError& error,
                                 blink::WebData& data);
  virtual void loadAsynchronously(const blink::WebURLRequest& request,
                                  blink::WebURLLoaderClient* client);
  virtual void cancel();
  virtual void setDefersLoading(bool defer);

  bool isDeferred() { return is_deferred_; }

 private:
  WebURLLoaderMockFactory* factory_;
  blink::WebURLLoaderClient* client_;
  scoped_ptr<blink::WebURLLoader> default_loader_;
  bool using_default_loader_;
  bool is_deferred_;
  bool* this_deleted_;

  DISALLOW_COPY_AND_ASSIGN(WebURLLoaderMock);
};

#endif  // CONTENT_TEST_WEBURL_LOADER_MOCK_H_
