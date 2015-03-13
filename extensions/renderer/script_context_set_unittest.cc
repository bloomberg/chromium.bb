// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop/message_loop.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace extensions {

TEST(ScriptContextSetTest, Lifecycle) {
  base::MessageLoop loop;

  ExtensionSet extensions;
  ExtensionIdSet active_extensions;
  ScriptContextSet context_set(&extensions, &active_extensions);

  blink::WebView* webview = blink::WebView::create(nullptr);
  blink::WebLocalFrame* frame = blink::WebLocalFrame::create(nullptr);
  webview->setMainFrame(frame);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> v8_context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(v8_context);
  ScriptContext* context = context_set.Register(
      frame, v8_context, 0, 0);  // no extension group or world ID

  // Context is valid and resembles correctness.
  EXPECT_TRUE(context->is_valid());
  EXPECT_EQ(frame, context->web_frame());
  EXPECT_EQ(v8_context, context->v8_context());

  // Context has been correctly added.
  EXPECT_EQ(1u, context_set.size());
  EXPECT_EQ(context, context_set.GetByV8Context(v8_context));

  // Test context is correctly removed.
  context_set.Remove(context);
  EXPECT_EQ(0u, context_set.size());
  EXPECT_EQ(nullptr, context_set.GetByV8Context(v8_context));

  // After removal, the context should be invalid.
  EXPECT_FALSE(context->is_valid());
  EXPECT_EQ(nullptr, context->web_frame());

  // Run loop to do the actual deletion.
  loop.RunUntilIdle();
}

}  // namespace extensions
