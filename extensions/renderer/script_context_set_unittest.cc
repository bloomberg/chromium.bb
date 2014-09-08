// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "extensions/common/extension.h"
#include "extensions/common/features/feature.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "gin/public/context_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "v8/include/v8.h"

namespace extensions {

TEST(ScriptContextSet, Lifecycle) {
  base::MessageLoop loop;

  ScriptContextSet context_set;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  gin::ContextHolder context_holder(isolate);
  context_holder.SetContext(v8::Context::New(isolate));

  // Dirty hack, but we don't actually need the frame, and this is easier than
  // creating a whole webview.
  blink::WebFrame* frame = reinterpret_cast<blink::WebFrame*>(1);
  const Extension* extension = NULL;
  ScriptContext* context =
      new ScriptContext(context_holder.context(),
                        frame,
                        extension,
                        Feature::BLESSED_EXTENSION_CONTEXT,
                        extension,
                        Feature::BLESSED_EXTENSION_CONTEXT);

  context_set.Add(context);
  EXPECT_EQ(1u, context_set.GetAll().count(context));
  EXPECT_EQ(context, context_set.GetByV8Context(context->v8_context()));

  // Adding the same item multiple times should be OK and deduped.
  context_set.Add(context);
  EXPECT_EQ(1u, context_set.GetAll().count(context));

  // GetAll() returns a copy so removing from one should not remove from others.
  ScriptContextSet::ContextSet set_copy = context_set.GetAll();
  EXPECT_EQ(1u, set_copy.count(context));

  context_set.Remove(context);
  EXPECT_EQ(0, context_set.size());
  EXPECT_FALSE(context_set.GetByV8Context(context->v8_context()));
  EXPECT_EQ(1u, set_copy.size());

  // After removal, the context should be marked for destruction.
  EXPECT_FALSE(context->web_frame());

  // Run loop to do the actual deletion.
  loop.RunUntilIdle();
}

}  // namespace extensions
