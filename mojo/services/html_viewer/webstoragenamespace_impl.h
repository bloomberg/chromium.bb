// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"

namespace mojo {

class WebStorageNamespaceImpl : public blink::WebStorageNamespace {
 public:
  WebStorageNamespaceImpl();

 private:
  virtual ~WebStorageNamespaceImpl();

  // blink::WebStorageNamespace methods:
  virtual blink::WebStorageArea* createStorageArea(
      const blink::WebString& origin);
  virtual bool isSameNamespace(const blink::WebStorageNamespace&) const;

  DISALLOW_COPY_AND_ASSIGN(WebStorageNamespaceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_
