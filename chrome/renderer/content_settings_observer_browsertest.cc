// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/test/render_view_test.h"
#include "content/common/view_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

// Regression test for http://crbug.com/35011
TEST_F(RenderViewTest, JSBlockSentAfterPageLoad) {
  // 1. Load page with JS.
  std::string html = "<html>"
           "<head>"
           "<script>document.createElement('div');</script>"
           "</head>"
           "<body>"
           "</body>"
           "</html>";
  render_thread_.sink().ClearMessages();
  LoadHTML(html.c_str());

  // 2. Block JavaScript.
  ContentSettings settings;
  for (int i = 0; i < CONTENT_SETTINGS_NUM_TYPES; ++i)
    settings.settings[i] = CONTENT_SETTING_ALLOW;
  settings.settings[CONTENT_SETTINGS_TYPE_JAVASCRIPT] = CONTENT_SETTING_BLOCK;
  ContentSettingsObserver* observer = ContentSettingsObserver::Get(view_);
  observer->SetContentSettings(settings);

  // Make sure no pending messages are in the queue.
  ProcessPendingMessages();
  render_thread_.sink().ClearMessages();

  // 3. Reload page.
  ViewMsg_Navigate_Params params;
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);
  params.url = url;
  params.navigation_type = ViewMsg_Navigate_Type::RELOAD;
  view_->OnNavigate(params);
  ProcessPendingMessages();

  // 4. Verify that the notification that javascript was blocked is sent after
  //    the navigation notifiction is sent.
  int navigation_index = -1;
  int block_index = -1;
  for (size_t i = 0; i < render_thread_.sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_.sink().GetMessageAt(i);
    if (msg->type() == ViewHostMsg_FrameNavigate::ID)
      navigation_index = i;
    if (msg->type() == ViewHostMsg_ContentBlocked::ID)
      block_index = i;
  }
  EXPECT_NE(-1, navigation_index);
  EXPECT_NE(-1, block_index);
  EXPECT_LT(navigation_index, block_index);
}
