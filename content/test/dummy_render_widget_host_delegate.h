// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DUMMY_RENDER_WIDGET_HOST_DELEGATE_H_
#define CONTENT_TEST_DUMMY_RENDER_WIDGET_HOST_DELEGATE_H_

#include "content/browser/renderer_host/render_widget_host_delegate.h"

namespace content {

// A RenderWidgetHostDelegate that does nothing.
class DummyRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  DummyRenderWidgetHostDelegate() {}
  ~DummyRenderWidgetHostDelegate() override {}

 private:
  // RenderWidgetHostDelegate:
  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void SelectAll() override {}
};

}  // namespace content

#endif  // CONTENT_TEST_DUMMY_RENDER_WIDGET_HOST_DELEGATE_H_
