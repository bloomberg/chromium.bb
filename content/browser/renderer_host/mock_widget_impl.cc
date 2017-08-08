// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/mock_widget_impl.h"

namespace content {

MockWidgetImpl::MockWidgetImpl(mojo::InterfaceRequest<mojom::Widget> request)
    : binding_(this, std::move(request)) {}

MockWidgetImpl::~MockWidgetImpl() {}

}  // namespace content
