// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_
#define CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/public/platform/WebVector.h"

namespace content {

class MockWebBlobRegistryImpl : public blink::WebBlobRegistry {
 public:
  MockWebBlobRegistryImpl();
  ~MockWebBlobRegistryImpl() override;

  void registerBlobData(const blink::WebString& uuid,
                        const blink::WebBlobData& data) override;
  void addBlobDataRef(const blink::WebString& uuid) override;
  void removeBlobDataRef(const blink::WebString& uuid) override;
  void registerPublicBlobURL(const blink::WebURL&,
                             const blink::WebString& uuid) override;
  void revokePublicBlobURL(const blink::WebURL&) override;

  // Additional support for Streams.
  void registerStreamURL(const blink::WebURL& url,
                         const blink::WebString& content_type) override;
  void registerStreamURL(const blink::WebURL& url,
                         const blink::WebURL& src_url) override;
  void addDataToStream(const blink::WebURL& url,
                       const char* data,
                       size_t length) override;
  void flushStream(const blink::WebURL& url) override;
  void finalizeStream(const blink::WebURL& url) override;
  void abortStream(const blink::WebURL& url) override;
  void unregisterStreamURL(const blink::WebURL& url) override;

  bool GetBlobItems(const blink::WebString& uuid,
                    blink::WebVector<blink::WebBlobData::Item*>* items) const;

 private:
  base::ScopedPtrHashMap<std::string,
                         scoped_ptr<ScopedVector<blink::WebBlobData::Item>>>
      blob_data_items_map_;
  std::map<std::string, int> blob_ref_count_map_;

  DISALLOW_COPY_AND_ASSIGN(MockWebBlobRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WEBBLOB_REGISTRY_IMPL_H_
