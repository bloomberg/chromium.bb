// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_
#define CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_

#include <stddef.h>

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"

namespace content {

class MockWebBlobRegistryImpl : public blink::WebBlobRegistry {
 public:
  MockWebBlobRegistryImpl();
  ~MockWebBlobRegistryImpl() override;

  void RegisterBlobData(const blink::WebString& uuid,
                        const blink::WebBlobData& data) override;
  std::unique_ptr<Builder> CreateBuilder(
      const blink::WebString& uuid,
      const blink::WebString& contentType) override;
  void AddBlobDataRef(const blink::WebString& uuid) override;
  void RemoveBlobDataRef(const blink::WebString& uuid) override;
  void RegisterPublicBlobURL(const blink::WebURL&,
                             const blink::WebString& uuid) override;
  void RevokePublicBlobURL(const blink::WebURL&) override;

  // Additional support for Streams.
  void RegisterStreamURL(const blink::WebURL& url,
                         const blink::WebString& content_type) override;
  void RegisterStreamURL(const blink::WebURL& url,
                         const blink::WebURL& src_url) override;
  void AddDataToStream(const blink::WebURL& url,
                       const char* data,
                       size_t length) override;
  void FlushStream(const blink::WebURL& url) override;
  void FinalizeStream(const blink::WebURL& url) override;
  void AbortStream(const blink::WebURL& url) override;
  void UnregisterStreamURL(const blink::WebURL& url) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockWebBlobRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_
