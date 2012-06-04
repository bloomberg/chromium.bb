// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/utf_string_conversions.h"
#include "content/browser/intents/intent_injector.h"
#include "content/browser/intents/internal_web_intents_dispatcher.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/web_contents/test_web_contents.h"
#include "content/common/intents_messages.h"
#include "content/common/view_messages.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/web_contents_tester.h"
#include "webkit/glue/web_intent_data.h"
#include "webkit/glue/web_intent_reply_data.h"

class IntentInjectorTest : public content::RenderViewHostImplTestHarness {
 public:
  IntentInjectorTest() {
    webkit_glue::WebIntentData intent(ASCIIToUTF16("action"),
                                      ASCIIToUTF16("type"),
                                      ASCIIToUTF16("unserialized_data"));
    // Will be destroyed when the IntentInjector shuts down.
    dispatcher_ = new InternalWebIntentsDispatcher(intent);
  }

  ~IntentInjectorTest() {}

  InternalWebIntentsDispatcher* dispatcher_;
};

TEST_F(IntentInjectorTest, TestDispatchLimitedToSameOrigin) {
  contents()->transition_cross_site = true;
  // Set up GetURL within the web_contents, and enable cross-site RVH
  // transitions.
  NavigateAndCommit(GURL("http://www.ddd.com/"));

  // Set up the injector, which will lock to the origin of GetURL.
  // The injector is a WebContentsObserver, destroyed when the
  // web_contents() is destroyed at the end of the test.
  IntentInjector* injector = new IntentInjector(web_contents());
  webkit_glue::WebIntentData intent(ASCIIToUTF16("action"),
                                    ASCIIToUTF16("type"),
                                    ASCIIToUTF16("unserialized_data"));
  injector->SetIntent(dispatcher_, intent);

  // Navigate to a same-origin page. The intent data should be sent.
  controller().LoadURL(GURL("http://www.ddd.com/page1"),
                       content::Referrer(),
                       content::PAGE_TRANSITION_TYPED,
                       std::string());
  test_rvh()->OnMessageReceived(ViewHostMsg_ShouldClose_ACK(
      rvh()->GetRoutingID(), true, base::TimeTicks(), base::TimeTicks()));

  ASSERT_FALSE(pending_rvh());
  injector->RenderViewCreated(rvh());

  EXPECT_FALSE(NULL == process()->sink().GetFirstMessageMatching(
      IntentsMsg_SetWebIntentData::ID));
  process()->sink().ClearMessages();

  // Intent data is sent to a different same-origin page.
  controller().LoadURL(GURL("http://www.ddd.com/page2"),
                       content::Referrer(),
                       content::PAGE_TRANSITION_TYPED,
                       std::string());
  test_rvh()->OnMessageReceived(ViewHostMsg_ShouldClose_ACK(
      rvh()->GetRoutingID(), true, base::TimeTicks(), base::TimeTicks()));
  ASSERT_FALSE(pending_rvh());
  injector->RenderViewCreated(rvh());
  EXPECT_FALSE(NULL == process()->sink().GetFirstMessageMatching(
      IntentsMsg_SetWebIntentData::ID));
  process()->sink().ClearMessages();

  controller().LoadURL(GURL("http://www.domain2.com/"),
                       content::Referrer(),
                       content::PAGE_TRANSITION_TYPED,
                       std::string());
  ASSERT_TRUE(contents()->cross_navigation_pending());
  ASSERT_TRUE(pending_rvh());
  injector->RenderViewCreated(pending_test_rvh());

  // The injector is still operating on the WebContents with the ddd.com
  // initial page, so no intent data is sent to this new renderer.
  EXPECT_TRUE(NULL == process()->sink().GetFirstMessageMatching(
      IntentsMsg_SetWebIntentData::ID));
}

TEST_F(IntentInjectorTest, AbandonDeletes) {
  IntentInjector* injector = new IntentInjector(web_contents());

  base::WeakPtr<IntentInjector> weak_injector =
      injector->weak_factory_.GetWeakPtr();
  EXPECT_FALSE(weak_injector.get() == NULL);
  weak_injector->Abandon();
  EXPECT_TRUE(weak_injector.get() == NULL);

  // Sending a message will both auto-delete |dispatcher_| to clean up and
  // verify that messages don't get sent to the (now deleted) |injector|.
  dispatcher_->SendReplyMessage(
      webkit_glue::WEB_INTENT_SERVICE_CONTENTS_CLOSED, string16());

}
