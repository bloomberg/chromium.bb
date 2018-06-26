// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/session_web_storage_namespace_impl.h"

#include "content/renderer/dom_storage/local_storage_area.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_url.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::WebStorageArea;
using blink::WebStorageNamespace;

namespace content {

SessionWebStorageNamespaceImpl::SessionWebStorageNamespaceImpl(
    const std::string& namespace_id,
    LocalStorageCachedAreas* local_storage_cached_areas)
    : namespace_id_(namespace_id),
      local_storage_cached_areas_(local_storage_cached_areas) {}

SessionWebStorageNamespaceImpl::~SessionWebStorageNamespaceImpl() {}

WebStorageArea* SessionWebStorageNamespaceImpl::CreateStorageArea(
    const blink::WebSecurityOrigin& origin) {
  return new LocalStorageArea(
      local_storage_cached_areas_->GetSessionStorageArea(namespace_id_,
                                                         origin));
}

blink::WebString SessionWebStorageNamespaceImpl::GetNamespaceId() const {
  return blink::WebString::FromASCII(namespace_id_);
}

bool SessionWebStorageNamespaceImpl::IsSameNamespace(
    const WebStorageNamespace& other) const {
  return GetNamespaceId() == other.GetNamespaceId();
}

}  // namespace content
