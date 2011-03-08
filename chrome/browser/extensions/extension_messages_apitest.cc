// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"

namespace {

class MessageSender : public NotificationObserver {
 public:
  MessageSender() {
    registrar_.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                   NotificationService::AllSources());
  }

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    ExtensionEventRouter* event_router =
        Source<Profile>(source).ptr()->GetExtensionEventRouter();

    // Sends four messages to the extension. All but the third message sent
    // from the origin http://b.com/ are supposed to arrive.
    event_router->DispatchEventToRenderers("test.onMessage",
        "[{\"lastMessage\":false,\"data\":\"no restriction\"}]",
        Source<Profile>(source).ptr(),
        GURL());
    event_router->DispatchEventToRenderers("test.onMessage",
        "[{\"lastMessage\":false,\"data\":\"http://a.com/\"}]",
        Source<Profile>(source).ptr(),
        GURL("http://a.com/"));
    event_router->DispatchEventToRenderers("test.onMessage",
        "[{\"lastMessage\":false,\"data\":\"http://b.com/\"}]",
        Source<Profile>(source).ptr(),
        GURL("http://b.com/"));
    event_router->DispatchEventToRenderers("test.onMessage",
        "[{\"lastMessage\":true,\"data\":\"last message\"}]",
        Source<Profile>(source).ptr(),
        GURL());
  }

  NotificationRegistrar registrar_;
};

}  // namespace

// Tests that message passing between extensions and content scripts works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Messaging) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("messaging/connect")) << message_;
}

// Tests that message passing from one extension to another works.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingExternal) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("..").AppendASCII("good")
                    .AppendASCII("Extensions")
                    .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
                    .AppendASCII("1.0")));

  ASSERT_TRUE(RunExtensionTest("messaging/connect_external")) << message_;
}

// Tests that messages with event_urls are only passed to extensions with
// appropriate permissions.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MessagingEventURL) {
  MessageSender sender;
  ASSERT_TRUE(RunExtensionTest("messaging/event_url")) << message_;
}
