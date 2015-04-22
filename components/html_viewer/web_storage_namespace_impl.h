// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_STORAGE_NAMESPACE_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_STORAGE_NAMESPACE_IMPL_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"

namespace html_viewer {

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

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_STORAGE_NAMESPACE_IMPL_H_
