// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_DOM_STORAGE_SESSION_WEB_STORAGE_NAMESPACE_IMPL_H_
#define CONTENT_RENDERER_DOM_STORAGE_SESSION_WEB_STORAGE_NAMESPACE_IMPL_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"

namespace content {
class LocalStorageCachedAreas;

class SessionWebStorageNamespaceImpl : public blink::WebStorageNamespace {
 public:
  // |local_storage_cached_areas| is guaranteed to outlive this object.
  SessionWebStorageNamespaceImpl(
      int64_t namespace_id,
      LocalStorageCachedAreas* local_storage_cached_areas);
  ~SessionWebStorageNamespaceImpl() override;

  // blink::WebStorageNamespace:
  blink::WebStorageArea* CreateStorageArea(
      const blink::WebSecurityOrigin& origin) override;
  bool IsSameNamespace(const WebStorageNamespace&) const override;

 private:
  int64_t namespace_id_;
  LocalStorageCachedAreas* const local_storage_cached_areas_;

  DISALLOW_COPY_AND_ASSIGN(SessionWebStorageNamespaceImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_DOM_STORAGE_SESSION_WEB_STORAGE_NAMESPACE_IMPL_H_
