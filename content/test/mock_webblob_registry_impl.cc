// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_webblob_registry_impl.h"

#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace content {

MockWebBlobRegistryImpl::MockWebBlobRegistryImpl() {
}

MockWebBlobRegistryImpl::~MockWebBlobRegistryImpl() {
}

void MockWebBlobRegistryImpl::registerBlobData(const WebString& uuid,
                                               const WebBlobData& data) {
  const std::string uuid_str(uuid.utf8());
  blob_ref_count_map_[uuid_str] = 1;
  scoped_ptr<ScopedVector<blink::WebBlobData::Item>> items(
      new ScopedVector<blink::WebBlobData::Item>);
  items->reserve(data.itemCount());
  for (size_t i = 0; i < data.itemCount(); ++i) {
    scoped_ptr<blink::WebBlobData::Item> item(new blink::WebBlobData::Item);
    data.itemAt(i, *item);
    items->push_back(item.release());
  }
  blob_data_items_map_.set(uuid_str, items.Pass());
}

void MockWebBlobRegistryImpl::addBlobDataRef(const WebString& uuid) {
  blob_ref_count_map_[uuid.utf8()]++;
}

void MockWebBlobRegistryImpl::removeBlobDataRef(const WebString& uuid) {
  const std::string uuid_str(uuid.utf8());
  auto it = blob_ref_count_map_.find(uuid_str);
  if (it != blob_ref_count_map_.end() && !--it->second) {
    blob_data_items_map_.erase(uuid_str);
    blob_ref_count_map_.erase(it);
  }
}

void MockWebBlobRegistryImpl::registerPublicBlobURL(const WebURL& url,
                                                    const WebString& uuid) {
}

void MockWebBlobRegistryImpl::revokePublicBlobURL(const WebURL& url) {
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const WebString& content_type) {
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const blink::WebURL& src_url) {
}

void MockWebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                              const char* data,
                                              size_t length) {
}

void MockWebBlobRegistryImpl::flushStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::finalizeStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::abortStream(const WebURL& url) {
}

void MockWebBlobRegistryImpl::unregisterStreamURL(const WebURL& url) {
}

bool MockWebBlobRegistryImpl::GetBlobItems(
    const WebString& uuid,
    WebVector<WebBlobData::Item*>* items) const {
  ScopedVector<WebBlobData::Item>* item_vector =
      blob_data_items_map_.get(uuid.utf8());
  if (!item_vector)
    return false;
  *items = item_vector->get();
  return true;
}

}  // namespace content
