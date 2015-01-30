// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_AX_PROVIDER_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_AX_PROVIDER_IMPL_H_

#include "third_party/mojo/src/mojo/public/cpp/bindings/interface_impl.h"
#include "third_party/mojo_services/src/accessibility/public/interfaces/accessibility.mojom.h"

namespace blink {
class WebAXObject;
class WebView;
}  // namespace blink

namespace html_viewer {

// Caller must ensure that |web_view| outlives AxProviderImpl.
class AxProviderImpl : public mojo::InterfaceImpl<mojo::AxProvider> {
 public:
  explicit AxProviderImpl(blink::WebView* web_view);
  void GetTree(const mojo::Callback<void(mojo::Array<mojo::AxNodePtr> nodes)>&
                   callback) override;

 private:
  int Populate(const blink::WebAXObject& ax_object,
               int parent_id,
               int next_sibling_id,
               mojo::Array<mojo::AxNodePtr>* result);
  mojo::AxNodePtr ConvertAxNode(const blink::WebAXObject& ax_object,
                                int parent_id,
                                int next_sibling_id);

  blink::WebView* web_view_;
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_AX_PROVIDER_IMPL_H_
