// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MOCK_WIDGET_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MOCK_WIDGET_IMPL_H_

#include "content/common/widget.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

class MockWidgetImpl : public mojom::Widget {
 public:
  explicit MockWidgetImpl(mojo::InterfaceRequest<mojom::Widget> request);
  ~MockWidgetImpl() override;

 private:
  mojo::Binding<mojom::Widget> binding_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MOCK_WIDGET_IMPL_H_
