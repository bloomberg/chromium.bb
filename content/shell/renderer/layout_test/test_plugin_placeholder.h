// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_PLACEHOLDER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_PLACEHOLDER_H_

#include "components/plugins/renderer/plugin_placeholder.h"

namespace content {

// A subclass of PluginPlaceholder for use in Blink layout tests.
class TestPluginPlaceholder : public plugins::PluginPlaceholder {
 public:
  TestPluginPlaceholder(blink::WebLocalFrame* frame,
                        const blink::WebPluginParams& params);

  void BindWebFrame(blink::WebFrame* frame) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TEST_PLUGIN_PLACEHOLDER_H_
