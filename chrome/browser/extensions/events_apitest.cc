// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Events) {
  ASSERT_TRUE(RunExtensionTest("events")) << message_;
}

// Fails on Win only. http://crbug.com/476863
#if defined(OS_WIN)
#define MAYBE_EventsAreUnregistered DISABLED_EventsAreUnregistered
#else
#define MAYBE_EventsAreUnregistered EventsAreUnregistered
#endif
// Tests that events are unregistered when an extension page shuts down.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_EventsAreUnregistered) {
  // In this test, page1.html registers for a number of events, then navigates
  // to page2.html, which should unregister those events. page2.html notifies
  // pass, by which point the event should have been unregistered.
  EventRouter* event_router = EventRouter::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  std::string test_extension_name = "events_are_unregistered";
  ASSERT_TRUE(RunExtensionSubtest(test_extension_name, "page1.html"))
      << message_;

  // Find the extension we just installed by looking for the path.
  const Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(),
                         test_data_dir_.AppendASCII(test_extension_name));
  ASSERT_TRUE(extension);
  std::string id = extension->id();

  // The page has closed, so no matter what all events are no longer listened
  // to.
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "browserAction.onClicked"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onStartup"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onSuspend"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onInstalled"));
}

}  // namespace extensions
