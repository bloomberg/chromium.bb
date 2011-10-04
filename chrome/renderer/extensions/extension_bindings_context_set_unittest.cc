// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/renderer/extensions/extension_bindings_context.h"
#include "chrome/renderer/extensions/extension_bindings_context_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "v8/include/v8.h"

TEST(ExtensionBindingsContextSet, Lifecycle) {
  MessageLoop loop;
  ExtensionBindingsContextSet::SetDeleteLoop(&loop);

  ExtensionBindingsContextSet context_set;

  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> v8_context(v8::Context::New());

  // Dirty hack, but we don't actually need the frame, and this is easier than
  // creating a whole webview.
  WebKit::WebFrame* frame = reinterpret_cast<WebKit::WebFrame*>(1);
  std::string extension_id = "00000000000000000000000000000000";
  ExtensionBindingsContext* context =
      new ExtensionBindingsContext(v8_context, frame, extension_id);

  context_set.Add(context);
  EXPECT_EQ(1u, context_set.GetAll().count(context));
  EXPECT_EQ(context, context_set.GetByV8Context(context->v8_context()));

  // Adding the same item multiple times should be OK and deduped.
  context_set.Add(context);
  EXPECT_EQ(1u, context_set.GetAll().count(context));

  // GetAll() returns a copy so removing from one should not remove from others.
  ExtensionBindingsContextSet::ContextSet set_copy = context_set.GetAll();
  EXPECT_EQ(1u, set_copy.count(context));

  context_set.Remove(context);
  EXPECT_EQ(0, context_set.size());
  EXPECT_FALSE(context_set.GetByV8Context(context->v8_context()));
  EXPECT_EQ(1u, set_copy.size());

  // After removal, the context should be marked for destruction.
  EXPECT_FALSE(context->web_frame());
}

TEST(ExtensionBindingsContextSet, RemoveByV8Context) {
  MessageLoop loop;
  ExtensionBindingsContextSet::SetDeleteLoop(&loop);

  ExtensionBindingsContextSet context_set;

  v8::HandleScope handle_scope;
  v8::Handle<v8::Context> v8_context(v8::Context::New());

  WebKit::WebFrame* frame = reinterpret_cast<WebKit::WebFrame*>(1);
  std::string extension_id = "00000000000000000000000000000000";
  ExtensionBindingsContext* context =
      new ExtensionBindingsContext(v8_context, frame, extension_id);

  context_set.Add(context);
  EXPECT_EQ(1, context_set.size());
  context_set.RemoveByV8Context(context->v8_context());
  EXPECT_EQ(0, context_set.size());
  EXPECT_FALSE(context->web_frame());
}
