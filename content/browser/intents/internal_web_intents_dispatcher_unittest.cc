// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "content/browser/intents/intent_injector.h"
#include "content/browser/intents/internal_web_intents_dispatcher.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class InternalWebIntentsDispatcherTest
    : public content::RenderViewHostTestHarness {
 public:
  InternalWebIntentsDispatcherTest()
      : replied_(0),
        notified_reply_type_(webkit_glue::WEB_INTENT_REPLY_INVALID) {
  }

  ~InternalWebIntentsDispatcherTest() {}

  void NotifyReply(webkit_glue::WebIntentReplyType reply_type,
                   const string16& data) {
    notified_reply_type_ = reply_type;
    notified_data_ = data;
  }

  void ReplySent(webkit_glue::WebIntentReplyType reply_type) {
    replied_++;
  }

  int replied_;
  string16 notified_data_;
  webkit_glue::WebIntentReplyType notified_reply_type_;
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

  dispatcher->SendReplyMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS,
                              ASCIIToUTF16("success"));

  EXPECT_EQ(ASCIIToUTF16("success"), notified_data_);
  EXPECT_EQ(1, replied_);
  EXPECT_EQ(webkit_glue::WEB_INTENT_REPLY_SUCCESS, notified_reply_type_);
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

  dispatcher->SendReplyMessage(webkit_glue::WEB_INTENT_REPLY_SUCCESS,
                              ASCIIToUTF16("success"));
}
