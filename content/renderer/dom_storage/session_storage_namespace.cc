// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/session_storage_namespace.h"

#include "content/renderer/dom_storage/local_storage_area.h"
#include "content/renderer/dom_storage/local_storage_cached_areas.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::WebStorageArea;
using blink::WebStorageNamespace;

namespace content {

SessionStorageNamespace::SessionStorageNamespace(
    int64_t namespace_id,
    LocalStorageCachedAreas* local_storage_cached_areas)
    : namespace_id_(namespace_id),
      local_storage_cached_areas_(local_storage_cached_areas) {}

SessionStorageNamespace::~SessionStorageNamespace() {}

WebStorageArea* SessionStorageNamespace::CreateStorageArea(
    const blink::WebSecurityOrigin& origin) {
  return new LocalStorageArea(
      local_storage_cached_areas_->GetSessionStorageArea(namespace_id_,
                                                         origin));
}

bool SessionStorageNamespace::IsSameNamespace(
    const WebStorageNamespace& other) const {
  const SessionStorageNamespace* other_impl =
      static_cast<const SessionStorageNamespace*>(&other);
  return namespace_id_ == other_impl->namespace_id_;
}

}  // namespace content
