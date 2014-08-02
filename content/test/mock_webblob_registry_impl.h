// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEB_BLOB_REGISTRY_IMPL_H
#define CONTENT_TEST_MOCK_WEB_BLOB_REGISTRY_IMPL_H

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"

namespace content {

class MockWebBlobRegistryImpl : public blink::WebBlobRegistry {
 public:
  MockWebBlobRegistryImpl();
  virtual ~MockWebBlobRegistryImpl();

  virtual void registerBlobData(const blink::WebString& uuid,
                                const blink::WebBlobData& data);
  virtual void addBlobDataRef(const blink::WebString& uuid);
  virtual void removeBlobDataRef(const blink::WebString& uuid);
  virtual void registerPublicBlobURL(const blink::WebURL&,
                                     const blink::WebString& uuid);
  virtual void revokePublicBlobURL(const blink::WebURL&);

  // Additional support for Streams.
  virtual void registerStreamURL(const blink::WebURL& url,
                                 const blink::WebString& content_type);
  virtual void registerStreamURL(const blink::WebURL& url,
                                 const blink::WebURL& src_url);
  virtual void addDataToStream(const blink::WebURL& url,
                               blink::WebThreadSafeData& data);
  virtual void finalizeStream(const blink::WebURL& url);
  virtual void abortStream(const blink::WebURL& url);
  virtual void unregisterStreamURL(const blink::WebURL& url);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebBlobRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEB_BLOB_REGISTRY_IMPL_H
