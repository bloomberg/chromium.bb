// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_webblob_registry_impl.h"

#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebURL;

namespace content {

MockWebBlobRegistryImpl::MockWebBlobRegistryImpl() {
}

MockWebBlobRegistryImpl::~MockWebBlobRegistryImpl() {
}

void MockWebBlobRegistryImpl::RegisterBlobData(const WebString& uuid,
                                               const WebBlobData& data) {}

std::unique_ptr<blink::WebBlobRegistry::Builder>
MockWebBlobRegistryImpl::CreateBuilder(const blink::WebString& uuid,
                                       const blink::WebString& contentType) {
  NOTREACHED();
  return nullptr;
}

void MockWebBlobRegistryImpl::AddBlobDataRef(const WebString& uuid) {}

void MockWebBlobRegistryImpl::RemoveBlobDataRef(const WebString& uuid) {}

void MockWebBlobRegistryImpl::RegisterPublicBlobURL(const WebURL& url,
                                                    const WebString& uuid) {}

void MockWebBlobRegistryImpl::RevokePublicBlobURL(const WebURL& url) {}

void MockWebBlobRegistryImpl::RegisterStreamURL(const WebURL& url,
                                                const WebString& content_type) {
}

void MockWebBlobRegistryImpl::RegisterStreamURL(const WebURL& url,
                                                const blink::WebURL& src_url) {}

void MockWebBlobRegistryImpl::AddDataToStream(const WebURL& url,
                                              const char* data,
                                              size_t length) {}

void MockWebBlobRegistryImpl::FlushStream(const WebURL& url) {}

void MockWebBlobRegistryImpl::FinalizeStream(const WebURL& url) {}

void MockWebBlobRegistryImpl::AbortStream(const WebURL& url) {}

void MockWebBlobRegistryImpl::UnregisterStreamURL(const WebURL& url) {}

}  // namespace content
