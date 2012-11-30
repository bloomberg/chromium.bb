// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "content/browser/intents/intent_injector.h"
#include "content/browser/intents/internal_web_intents_dispatcher.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

namespace content {

class InternalWebIntentsDispatcherTest : public RenderViewHostTestHarness {
 public:
  InternalWebIntentsDispatcherTest() : reply_count_(0) {
  }

  ~InternalWebIntentsDispatcherTest() {}

  void NotifyReply(const webkit_glue::WebIntentReply& reply) {
    reply_.reset(new webkit_glue::WebIntentReply(reply));
  }

  void ReplySent(webkit_glue::WebIntentReplyType reply_type) {
    reply_count_++;
  }

  int reply_count_;
  scoped_ptr<const webkit_glue::WebIntentReply> reply_;
};

// Tests that the internal dispatcher correctly notifies
// its callbacks of any reply messages.
TEST_F(InternalWebIntentsDispatcherTest, NotifiesOnReply) {
  webkit_glue::WebIntentData intent(ASCIIToUTF16("action"),
                                    ASCIIToUTF16("type"),
                                    ASCIIToUTF16("unserialized_data"));
  InternalWebIntentsDispatcher* dispatcher = new InternalWebIntentsDispatcher(
      intent, base::Bind(&InternalWebIntentsDispatcherTest::NotifyReply,
                         base::Unretained(this)));
  dispatcher->RegisterReplyNotification(
      base::Bind(&InternalWebIntentsDispatcherTest::ReplySent,
                 base::Unretained(this)));

  dispatcher->DispatchIntent(web_contents());

  webkit_glue::WebIntentReply reply(
        webkit_glue::WEB_INTENT_REPLY_SUCCESS, ASCIIToUTF16("success"));
  dispatcher->SendReply(reply);

  ASSERT_TRUE(1 == reply_count_);
  ASSERT_TRUE(reply_);
  EXPECT_EQ(*reply_.get(), reply);
}

TEST_F(InternalWebIntentsDispatcherTest, CancelAbandonsInjector) {
  webkit_glue::WebIntentData intent(ASCIIToUTF16("action"),
                                    ASCIIToUTF16("type"),
                                    ASCIIToUTF16("unserialized_data"));
  InternalWebIntentsDispatcher* dispatcher = new InternalWebIntentsDispatcher(
      intent, base::Bind(&InternalWebIntentsDispatcherTest::NotifyReply,
                         base::Unretained(this)));
  dispatcher->DispatchIntent(web_contents());
  EXPECT_FALSE(dispatcher->intent_injector_ == NULL);
  dispatcher->ResetDispatch();
  EXPECT_TRUE(dispatcher->intent_injector_ == NULL);

  dispatcher->SendReply(webkit_glue::WebIntentReply(
      webkit_glue::WEB_INTENT_REPLY_SUCCESS, ASCIIToUTF16("success")));
}

}  // namespace content
