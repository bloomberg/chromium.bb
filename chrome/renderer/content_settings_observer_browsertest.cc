// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/content_settings_observer.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_message_macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/web/WebView.h"

using testing::_;
using testing::DeleteArg;

namespace {

class MockContentSettingsObserver : public ContentSettingsObserver {
 public:
  explicit MockContentSettingsObserver(content::RenderFrame* render_frame);

  virtual bool Send(IPC::Message* message);

  MOCK_METHOD1(OnContentBlocked,
               void(ContentSettingsType));

  MOCK_METHOD5(OnAllowDOMStorage,
               void(int, const GURL&, const GURL&, bool, IPC::Message*));
  GURL image_url_;
  std::string image_origin_;
};

MockContentSettingsObserver::MockContentSettingsObserver(
    content::RenderFrame* render_frame)
    : ContentSettingsObserver(render_frame, NULL),
      image_url_("http://www.foo.com/image.jpg"),
      image_origin_("http://www.foo.com") {
}

bool MockContentSettingsObserver::Send(IPC::Message* message) {
  IPC_BEGIN_MESSAGE_MAP(MockContentSettingsObserver, *message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_ContentBlocked, OnContentBlocked)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ChromeViewHostMsg_AllowDOMStorage,
                                    OnAllowDOMStorage)
    IPC_MESSAGE_UNHANDLED(ADD_FAILURE())
  IPC_END_MESSAGE_MAP()

  // Our super class deletes the message.
  return RenderFrameObserver::Send(message);
}

}  // namespace

TEST_F(ChromeRenderViewTest, DidBlockContentType) {
  MockContentSettingsObserver observer(view_->GetMainRenderFrame());
  EXPECT_CALL(observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_COOKIES));
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES);

  // Blocking the same content type a second time shouldn't send a notification.
  observer.DidBlockContentType(CONTENT_SETTINGS_TYPE_COOKIES);
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

// Tests that multiple invokations of AllowDOMStorage result in a single IPC.
// Fails due to http://crbug.com/104300
TEST_F(ChromeRenderViewTest, DISABLED_AllowDOMStorage) {
  // Load some HTML, so we have a valid security origin.
  LoadHTML("<html></html>");
  MockContentSettingsObserver observer(view_->GetMainRenderFrame());
  ON_CALL(observer,
          OnAllowDOMStorage(_, _, _, _, _)).WillByDefault(DeleteArg<4>());
  EXPECT_CALL(observer,
              OnAllowDOMStorage(_, _, _, _, _));
  observer.allowStorage(true);

  // Accessing localStorage from the same origin again shouldn't result in a
  // new IPC.
  observer.allowStorage(true);
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

// Regression test for http://crbug.com/35011
TEST_F(ChromeRenderViewTest, JSBlockSentAfterPageLoad) {
  // 1. Load page with JS.
  std::string html = "<html>"
                     "<head>"
                     "<script>document.createElement('div');</script>"
                     "</head>"
                     "<body>"
                     "</body>"
                     "</html>";
  render_thread_->sink().ClearMessages();
  LoadHTML(html.c_str());

  // 2. Block JavaScript.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_BLOCK,
                                  std::string(),
                                  false));
  ContentSettingsObserver* observer = ContentSettingsObserver::Get(
      view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);

  // Make sure no pending messages are in the queue.
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  // 3. Reload page.
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);
  Reload(url);
  ProcessPendingMessages();

  // 4. Verify that the notification that javascript was blocked is sent after
  //    the navigation notification is sent.
  int navigation_index = -1;
  int block_index = -1;
  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
    if (msg->type() == GetNavigationIPCType())
      navigation_index = i;
    if (msg->type() == ChromeViewHostMsg_ContentBlocked::ID)
      block_index = i;
  }
  EXPECT_NE(-1, navigation_index);
  EXPECT_NE(-1, block_index);
  EXPECT_LT(navigation_index, block_index);
}

TEST_F(ChromeRenderViewTest, PluginsTemporarilyAllowed) {
  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  std::string foo_plugin = "foo";
  std::string bar_plugin = "bar";

  ContentSettingsObserver* observer =
      ContentSettingsObserver::Get(view_->GetMainRenderFrame());
  EXPECT_FALSE(observer->IsPluginTemporarilyAllowed(foo_plugin));

  // Temporarily allow the "foo" plugin.
  observer->OnLoadBlockedPlugins(foo_plugin);
  EXPECT_TRUE(observer->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(observer->IsPluginTemporarilyAllowed(bar_plugin));

  // Simulate a navigation within the page.
  DidNavigateWithinPage(GetMainFrame(), true);
  EXPECT_TRUE(observer->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(observer->IsPluginTemporarilyAllowed(bar_plugin));

  // Navigate to a different page.
  LoadHTML("<html>Bar</html>");
  EXPECT_FALSE(observer->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_FALSE(observer->IsPluginTemporarilyAllowed(bar_plugin));

  // Temporarily allow all plugins.
  observer->OnLoadBlockedPlugins(std::string());
  EXPECT_TRUE(observer->IsPluginTemporarilyAllowed(foo_plugin));
  EXPECT_TRUE(observer->IsPluginTemporarilyAllowed(bar_plugin));
}

TEST_F(ChromeRenderViewTest, ImagesBlockedByDefault) {
  MockContentSettingsObserver mock_observer(view_->GetMainRenderFrame());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_BLOCK,
                                  std::string(),
                                  false));

  ContentSettingsObserver* observer = ContentSettingsObserver::Get(
      view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);
  EXPECT_CALL(mock_observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(observer->allowImage(true, mock_observer.image_url_));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Create an exception which allows the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_observer.image_origin_),
          CONTENT_SETTING_ALLOW,
          std::string(),
          false));

  EXPECT_CALL(
      mock_observer,
      OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES)).Times(0);
  EXPECT_TRUE(observer->allowImage(true, mock_observer.image_url_));
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(ChromeRenderViewTest, ImagesAllowedByDefault) {
  MockContentSettingsObserver mock_observer(view_->GetMainRenderFrame());

  // Load some HTML.
  LoadHTML("<html>Foo</html>");

  // Set the default image blocking setting.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  false));

  ContentSettingsObserver* observer =
      ContentSettingsObserver::Get(view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);
  EXPECT_CALL(
      mock_observer,
      OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES)).Times(0);
  EXPECT_TRUE(observer->allowImage(true, mock_observer.image_url_));
  ::testing::Mock::VerifyAndClearExpectations(&observer);

  // Create an exception which blocks the image.
  image_setting_rules.insert(
      image_setting_rules.begin(),
      ContentSettingPatternSource(
          ContentSettingsPattern::Wildcard(),
          ContentSettingsPattern::FromString(mock_observer.image_origin_),
          CONTENT_SETTING_BLOCK,
          std::string(),
          false));
  EXPECT_CALL(mock_observer,
              OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES));
  EXPECT_FALSE(observer->allowImage(true, mock_observer.image_url_));
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(ChromeRenderViewTest, ContentSettingsBlockScripts) {
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_BLOCK,
                                  std::string(),
                                  false));

  ContentSettingsObserver* observer =
      ContentSettingsObserver::Get(view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  std::string html = "<html>"
                     "<head>"
                     "<script src='data:foo'></script>"
                     "</head>"
                     "<body>"
                     "</body>"
                     "</html>";
  LoadHTML(html.c_str());

  // Verify that the script was blocked.
  bool was_blocked = false;
  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
    if (msg->type() == ChromeViewHostMsg_ContentBlocked::ID)
      was_blocked = true;
  }
  EXPECT_TRUE(was_blocked);
}

TEST_F(ChromeRenderViewTest, ContentSettingsAllowScripts) {
  // Set the content settings for scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_ALLOW,
                                  std::string(),
                                  false));

  ContentSettingsObserver* observer =
      ContentSettingsObserver::Get(view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);

  // Load a page which contains a script.
  std::string html = "<html>"
                     "<head>"
                     "<script src='data:foo'></script>"
                     "</head>"
                     "<body>"
                     "</body>"
                     "</html>";
  LoadHTML(html.c_str());

  // Verify that the script was not blocked.
  bool was_blocked = false;
  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
    if (msg->type() == ChromeViewHostMsg_ContentBlocked::ID)
      was_blocked = true;
  }
  EXPECT_FALSE(was_blocked);
}

TEST_F(ChromeRenderViewTest, ContentSettingsInterstitialPages) {
  MockContentSettingsObserver mock_observer(view_->GetMainRenderFrame());
  // Block scripts.
  RendererContentSettingRules content_setting_rules;
  ContentSettingsForOneType& script_setting_rules =
      content_setting_rules.script_rules;
  script_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_BLOCK,
                                  std::string(),
                                  false));
  // Block images.
  ContentSettingsForOneType& image_setting_rules =
      content_setting_rules.image_rules;
  image_setting_rules.push_back(
      ContentSettingPatternSource(ContentSettingsPattern::Wildcard(),
                                  ContentSettingsPattern::Wildcard(),
                                  CONTENT_SETTING_BLOCK,
                                  std::string(),
                                  false));

  ContentSettingsObserver* observer =
      ContentSettingsObserver::Get(view_->GetMainRenderFrame());
  observer->SetContentSettingRules(&content_setting_rules);
  observer->OnSetAsInterstitial();

  // Load a page which contains a script.
  std::string html = "<html>"
                     "<head>"
                     "<script src='data:foo'></script>"
                     "</head>"
                     "<body>"
                     "</body>"
                     "</html>";
  LoadHTML(html.c_str());

  // Verify that the script was allowed.
  bool was_blocked = false;
  for (size_t i = 0; i < render_thread_->sink().message_count(); ++i) {
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(i);
    if (msg->type() == ChromeViewHostMsg_ContentBlocked::ID)
      was_blocked = true;
  }
  EXPECT_FALSE(was_blocked);

  // Verify that images are allowed.
  EXPECT_CALL(
      mock_observer,
      OnContentBlocked(CONTENT_SETTINGS_TYPE_IMAGES)).Times(0);
  EXPECT_TRUE(observer->allowImage(true, mock_observer.image_url_));
  ::testing::Mock::VerifyAndClearExpectations(&observer);
}
