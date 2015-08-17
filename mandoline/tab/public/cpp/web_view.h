// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_TAB_PUBLIC_CPP_WEB_VIEW_H_
#define MANDOLINE_TAB_PUBLIC_CPP_WEB_VIEW_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mandoline/tab/public/interfaces/web_view.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"

namespace mojo {
class ApplicationImpl;
class View;
}

namespace web_view {

class WebView {
 public:
  explicit WebView(mojom::WebViewClient* client);
  ~WebView();

  void Init(mojo::ApplicationImpl* app, mojo::View* view);

  mojom::WebView* web_view() { return web_view_.get(); }

 private:
  mojom::WebViewPtr web_view_;
  mojo::Binding<mojom::WebViewClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(WebView);
};

}  // namespace web_view

#endif  // MANDOLINE_TAB_PUBLIC_CPP_WEB_VIEW_H_
