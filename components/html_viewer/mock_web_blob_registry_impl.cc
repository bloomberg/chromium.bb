// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/mock_web_blob_registry_impl.h"

#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebURL;
using blink::WebVector;

namespace html_viewer {

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
  public_url_to_uuid_[url.spec()] = uuid;
  addBlobDataRef(uuid);
}

void MockWebBlobRegistryImpl::revokePublicBlobURL(const WebURL& url) {
  auto it = public_url_to_uuid_.find(url.spec());
  if (it != public_url_to_uuid_.end()) {
    removeBlobDataRef(it->second);
    public_url_to_uuid_.erase(it);
  }
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const WebString& content_type) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::registerStreamURL(const WebURL& url,
                                                const blink::WebURL& src_url) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::addDataToStream(const WebURL& url,
                                              const char* data,
                                              size_t length) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::flushStream(const WebURL& url) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::finalizeStream(const WebURL& url) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::abortStream(const WebURL& url) {
  NOTIMPLEMENTED();
}

void MockWebBlobRegistryImpl::unregisterStreamURL(const WebURL& url) {
  NOTIMPLEMENTED();
}

bool MockWebBlobRegistryImpl::GetUUIDForURL(const blink::WebURL& url,
                                            blink::WebString* uuid) const {
  auto it = public_url_to_uuid_.find(url.spec());
  if (it != public_url_to_uuid_.end()) {
    *uuid = it->second;
    return true;
  }

  return false;
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

}  // namespace html_viewer
