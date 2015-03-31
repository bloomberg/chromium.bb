// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_

#include "base/macros.h"
#include "third_party/WebKit/public/platform/WebStorageNamespace.h"

namespace html_viewer {

class WebStorageNamespaceImpl : public blink::WebStorageNamespace {
 public:
  WebStorageNamespaceImpl();

 private:
  ~WebStorageNamespaceImpl() override;

  // blink::WebStorageNamespace methods:
  blink::WebStorageArea* createStorageArea(
      const blink::WebString& origin) override;
  bool isSameNamespace(const blink::WebStorageNamespace&) const override;

  DISALLOW_COPY_AND_ASSIGN(WebStorageNamespaceImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBSTORAGENAMESPACE_IMPL_H_
