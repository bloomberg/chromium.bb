// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_browsertest.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "content/common/browser_plugin_messages.h"
#include "content/public/common/content_constants.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_factory.h"
#include "content/renderer/browser_plugin/mock_browser_plugin.h"
#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"

namespace {
const char kHTMLForBrowserPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    " src='foo' type='%s'>";

const char kHTMLForBrowserPluginWithAllAttributes[] =
    "<object id='browserplugin' width='640' height='480' type='%s'"
    " autosize maxheight='600' maxwidth='800' minheight='240'"
    " minwidth='320' name='Jim' partition='someid' src='foo'>";

const char kHTMLForSourcelessPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px' type='%s'>";

const char kHTMLForPartitionedPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    "  src='foo' type='%s' partition='someid'>";

const char kHTMLForInvalidPartitionedPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    "  type='%s' partition='persist:'>";

const char kHTMLForPartitionedPersistedPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    "  src='foo' type='%s' partition='persist:someid'>";

std::string GetHTMLForBrowserPluginObject() {
  return StringPrintf(kHTMLForBrowserPluginObject,
                      content::kBrowserPluginMimeType);
}

}  // namespace

namespace content {

class TestContentRendererClient : public ContentRendererClient {
 public:
  TestContentRendererClient() : ContentRendererClient() {
  }
  virtual ~TestContentRendererClient() {
  }
  virtual bool AllowBrowserPlugin(WebKit::WebPluginContainer* container) const
      OVERRIDE {
    // Allow BrowserPlugin for tests.
    return true;
  }
};

// Test factory for creating test instances of BrowserPluginManager.
class TestBrowserPluginManagerFactory : public BrowserPluginManagerFactory {
 public:
  virtual MockBrowserPluginManager* CreateBrowserPluginManager(
      RenderViewImpl* render_view) OVERRIDE {
    return new MockBrowserPluginManager(render_view);
  }

  // Singleton getter.
  static TestBrowserPluginManagerFactory* GetInstance() {
    return Singleton<TestBrowserPluginManagerFactory>::get();
  }

 protected:
  TestBrowserPluginManagerFactory() {}
  virtual ~TestBrowserPluginManagerFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginManagerFactory>;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginManagerFactory);
};

BrowserPluginTest::BrowserPluginTest() {}

BrowserPluginTest::~BrowserPluginTest() {}

void BrowserPluginTest::SetUp() {
  test_content_renderer_client_.reset(new TestContentRendererClient);
  GetContentClient()->set_renderer_for_testing(
      test_content_renderer_client_.get());
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
  content::RenderViewTest::SetUp();
}

void BrowserPluginTest::TearDown() {
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
  content::RenderViewTest::TearDown();
  test_content_renderer_client_.reset();
}

std::string BrowserPluginTest::ExecuteScriptAndReturnString(
    const std::string& script) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      WebKit::WebScriptSource(WebKit::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsString())
    return std::string();

  v8::Local<v8::String> v8_str = value->ToString();
  int length = v8_str->Utf8Length() + 1;
  scoped_array<char> str(new char[length]);
  v8_str->WriteUtf8(str.get(), length);
  return str.get();
}

int BrowserPluginTest::ExecuteScriptAndReturnInt(
    const std::string& script) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      WebKit::WebScriptSource(WebKit::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsInt32())
    return 0;

  return value->Int32Value();
}

// A return value of false means that a value was not present. The return value
// of the script is stored in |result|
bool BrowserPluginTest::ExecuteScriptAndReturnBool(
    const std::string& script, bool* result) {
  v8::HandleScope handle_scope;
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      WebKit::WebScriptSource(WebKit::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsBoolean())
    return false;

  *result = value->BooleanValue();
  return true;
}

// This test verifies that an initial resize occurs when we instantiate the
// browser plugin. This test also verifies that the browser plugin is waiting
// for a BrowserPluginMsg_UpdateRect in response. We issue an UpdateRect, and
// we observe an UpdateRect_ACK, with the |pending_damage_buffer_| reset,
// indiciating that the BrowserPlugin is not waiting for any more UpdateRects to
// satisfy its resize request.
TEST_F(BrowserPluginTest, InitialResize) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  // Verify that the information in CreateGuest is correct.
  int instance_id = 0;
  {
    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(msg);
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);
    EXPECT_EQ(640, params.resize_guest_params.view_size.width());
    EXPECT_EQ(480, params.resize_guest_params.view_size.height());
  }

  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);
  // Now the browser plugin is expecting a UpdateRect resize.
  EXPECT_TRUE(browser_plugin->pending_damage_buffer_.get());

  // Send the BrowserPlugin an UpdateRect equal to its container size with
  // the same damage buffer. That should clear |pending_damage_buffer_|.
  BrowserPluginMsg_UpdateRect_Params update_rect_params;
  update_rect_params.damage_buffer_sequence_id =
      browser_plugin->damage_buffer_sequence_id_;
  update_rect_params.view_size = gfx::Size(640, 480);
  update_rect_params.scale_factor = 1.0f;
  update_rect_params.is_resize_ack = true;
  update_rect_params.needs_ack = true;
  BrowserPluginMsg_UpdateRect msg(0, instance_id, update_rect_params);
  browser_plugin->OnMessageReceived(msg);
  EXPECT_FALSE(browser_plugin->pending_damage_buffer_.get());
}

// This test verifies that all attributes (present at the time of writing) are
// parsed on initialization. However, this test does minimal checking of
// correct behavior.
TEST_F(BrowserPluginTest, ParseAllAttributes) {
  std::string html = StringPrintf(kHTMLForBrowserPluginWithAllAttributes,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  bool result;
  bool has_value = ExecuteScriptAndReturnBool(
      "document.getElementById('browserplugin').autosize", &result);
  EXPECT_TRUE(has_value);
  EXPECT_TRUE(result);
  int maxHeight = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').maxheight");
  EXPECT_EQ(600, maxHeight);
  int maxWidth = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').maxwidth");
  EXPECT_EQ(800, maxWidth);
  int minHeight = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').minheight");
  EXPECT_EQ(240, minHeight);
  int minWidth = ExecuteScriptAndReturnInt(
      "document.getElementById('browserplugin').minwidth");
  EXPECT_EQ(320, minWidth);
  std::string name = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').name");
  EXPECT_STREQ("Jim", name.c_str());
  std::string partition = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("someid", partition.c_str());
  std::string src = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').src");
  EXPECT_STREQ("foo", src.c_str());
}

// Verify that the src attribute on the browser plugin works as expected.
TEST_F(BrowserPluginTest, SrcAttribute) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  // Verify that we're reporting the correct URL to navigate to based on the
  // src attribute.
  {
    // Ensure we get a CreateGuest on the initial navigation.
    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(msg);

    int instance_id = 0;
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);
    EXPECT_EQ("foo", params.src);
  }

  browser_plugin_manager()->sink().ClearMessages();
  // Navigate to bar and observe the associated
  // BrowserPluginHostMsg_NavigateGuest message.
  // Verify that the src attribute is updated as well.
  ExecuteJavaScript("document.getElementById('browserplugin').src = 'bar'");
  {
    // Verify that we do not get a CreateGuest on subsequent navigations.
    const IPC::Message* create_msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_FALSE(create_msg);

    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_NavigateGuest::ID);
    ASSERT_TRUE(msg);

    int instance_id = 0;
    std::string src;
    BrowserPluginHostMsg_NavigateGuest::Read(msg, &instance_id, &src);
    EXPECT_EQ("bar", src);
    std::string src_value =
        ExecuteScriptAndReturnString(
            "document.getElementById('browserplugin').src");
    EXPECT_EQ("bar", src_value);
  }
}

TEST_F(BrowserPluginTest, ResizeFlowControl) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  int instance_id = 0;
  {
    // Ensure we get a CreateGuest on the initial navigation and grab the
    // BrowserPlugin's instance_id from there.
    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(msg);
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);
  }
  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);
  EXPECT_TRUE(browser_plugin->pending_damage_buffer_.get());
  // Send an UpdateRect to the BrowserPlugin to make it use the pending damage
  // buffer.
  {
    // We send a stale UpdateRect to the BrowserPlugin.
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(640, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    update_rect_params.needs_ack = true;
    // By sending |damage_buffer_sequence_id| back to BrowserPlugin on
    // UpdateRect, then the BrowserPlugin knows that the browser process has
    // received and has begun to use the |pending_damage_buffer_|.
    update_rect_params.damage_buffer_sequence_id =
        browser_plugin->damage_buffer_sequence_id_;
    BrowserPluginMsg_UpdateRect msg(0, instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
    EXPECT_EQ(NULL, browser_plugin->pending_damage_buffer_.get());
  }

  browser_plugin_manager()->sink().ClearMessages();

  // Resize the browser plugin three times.
  ExecuteJavaScript("document.getElementById('browserplugin').width = '641px'");
  ProcessPendingMessages();
  ExecuteJavaScript("document.getElementById('browserplugin').width = '642px'");
  ProcessPendingMessages();
  ExecuteJavaScript("document.getElementById('browserplugin').width = '643px'");
  ProcessPendingMessages();

  // Expect to see one messsage in the sink. BrowserPlugin will not issue
  // subsequent resize requests until the first request is satisfied by the
  // guest.
  EXPECT_EQ(1u, browser_plugin_manager()->sink().message_count());
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetFirstMessageMatching(
          BrowserPluginHostMsg_ResizeGuest::ID);
  ASSERT_TRUE(msg);
  BrowserPluginHostMsg_ResizeGuest_Params params;
  BrowserPluginHostMsg_ResizeGuest::Read(msg, &instance_id, &params);
  EXPECT_EQ(641, params.view_size.width());
  EXPECT_EQ(480, params.view_size.height());
  // This indicates that the BrowserPlugin has sent out a previous resize
  // request but has not yet received an UpdateRect for that request.
  EXPECT_TRUE(browser_plugin->pending_damage_buffer_.get());

  {
    // We send a stale UpdateRect to the BrowserPlugin.
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(641, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    update_rect_params.needs_ack = true;
    update_rect_params.damage_buffer_sequence_id =
        browser_plugin->damage_buffer_sequence_id_;
    BrowserPluginMsg_UpdateRect msg(0, instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
    // This tells us that the BrowserPlugin is still expecting another
    // UpdateRect with the most recent size.
    EXPECT_TRUE(browser_plugin->pending_damage_buffer_.get());
  }
  // Send the BrowserPlugin another UpdateRect, but this time with a size
  // that matches the size of the container.
  {
    BrowserPluginMsg_UpdateRect_Params update_rect_params;
    update_rect_params.view_size = gfx::Size(643, 480);
    update_rect_params.scale_factor = 1.0f;
    update_rect_params.is_resize_ack = true;
    update_rect_params.needs_ack = true;
    update_rect_params.damage_buffer_sequence_id =
        browser_plugin->damage_buffer_sequence_id_;
    BrowserPluginMsg_UpdateRect msg(0, instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
    // The BrowserPlugin has finally received an UpdateRect that satisifes
    // its current size, and so it is happy.
    EXPECT_FALSE(browser_plugin->pending_damage_buffer_.get());
  }
}

TEST_F(BrowserPluginTest, GuestCrash) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());

  // Grab the BrowserPlugin's instance ID from its CreateGuest message.
  int instance_id = 0;
  {
    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetFirstMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(msg);
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);
  }
  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);

  WebKit::WebCursorInfo cursor_info;
  // Send an event and verify that the event is deported.
  browser_plugin->handleInputEvent(WebKit::WebMouseEvent(),
                                   cursor_info);
  EXPECT_TRUE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_HandleInputEvent::ID));
  browser_plugin_manager()->sink().ClearMessages();

  const char* kAddEventListener =
    "var msg;"
    "function exitListener(e) {"
    "  msg = JSON.parse(e.detail).reason;"
    "}"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-exit', exitListener);";

  ExecuteJavaScript(kAddEventListener);

  // Pretend that the guest has terminated normally.
  {
    BrowserPluginMsg_GuestGone msg(
        0, 0, 0, base::TERMINATION_STATUS_NORMAL_TERMINATION);
    browser_plugin->OnMessageReceived(msg);
  }

  // Verify that our event listener has fired.
  EXPECT_EQ("normal", ExecuteScriptAndReturnString("msg"));

  // Pretend that the guest has crashed.
  {
    BrowserPluginMsg_GuestGone msg(
        0, 0, 0, base::TERMINATION_STATUS_PROCESS_CRASHED);
    browser_plugin->OnMessageReceived(msg);
  }

  // Verify that our event listener has fired.
  EXPECT_EQ("crashed", ExecuteScriptAndReturnString("msg"));

  // Send an event and verify that events are no longer deported.
  browser_plugin->handleInputEvent(WebKit::WebMouseEvent(),
                                   cursor_info);
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_HandleInputEvent::ID));
}

TEST_F(BrowserPluginTest, RemovePlugin) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
  ExecuteJavaScript("x = document.getElementById('browserplugin'); "
                    "x.parentNode.removeChild(x);");
  ProcessPendingMessages();
  EXPECT_TRUE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
}

// This test verifies that PluginDestroyed messages do not get sent from a
// BrowserPlugin that has never navigated.
TEST_F(BrowserPluginTest, RemovePluginBeforeNavigation) {
  std::string html = StringPrintf(kHTMLForSourcelessPluginObject,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
  ExecuteJavaScript("x = document.getElementById('browserplugin'); "
                    "x.parentNode.removeChild(x);");
  ProcessPendingMessages();
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
}

TEST_F(BrowserPluginTest, CustomEvents) {
  const char* kAddEventListener =
    "var url;"
    "function nav(e) {"
    "  url = JSON.parse(e.detail).url;"
    "}"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-loadcommit', nav);";
  const char* kRemoveEventListener =
    "document.getElementById('browserplugin')."
    "    removeEventListener('-internal-loadcommit', nav);";
  const char* kGetProcessID =
      "document.getElementById('browserplugin').getProcessId()";
  const char* kGetSrc =
      "document.getElementById('browserplugin').src";
  const char* kGoogleURL = "http://www.google.com/";
  const char* kGoogleNewsURL = "http://news.google.com/";

  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  ExecuteJavaScript(kAddEventListener);
  // Grab the BrowserPlugin's instance ID from its resize message.
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetFirstMessageMatching(
          BrowserPluginHostMsg_CreateGuest::ID);
  ASSERT_TRUE(msg);
  int instance_id = 0;
  BrowserPluginHostMsg_CreateGuest_Params params;
  BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);

  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);

  {
    BrowserPluginMsg_LoadCommit_Params navigate_params;
    navigate_params.is_top_level = true;
    navigate_params.url = GURL(kGoogleURL);
    navigate_params.process_id = 1337;
    BrowserPluginMsg_LoadCommit msg(0, instance_id, navigate_params);
    browser_plugin->OnMessageReceived(msg);
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString("url"));
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString(kGetSrc));
    EXPECT_EQ(1337, ExecuteScriptAndReturnInt(kGetProcessID));
  }
  ExecuteJavaScript(kRemoveEventListener);
  {
    BrowserPluginMsg_LoadCommit_Params navigate_params;
    navigate_params.is_top_level = false;
    navigate_params.url = GURL(kGoogleNewsURL);
    navigate_params.process_id = 42;
    BrowserPluginMsg_LoadCommit msg(0, instance_id, navigate_params);
    browser_plugin->OnMessageReceived(msg);
    // The URL variable should not change because we've removed the event
    // listener.
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString("url"));
    // The src attribute should not change if this is a top-level navigation.
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString(kGetSrc));
    EXPECT_EQ(42, ExecuteScriptAndReturnInt(kGetProcessID));
  }
}

TEST_F(BrowserPluginTest, StopMethod) {
  const char* kCallStop =
    "document.getElementById('browserplugin').stop();";
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  ExecuteJavaScript(kCallStop);
  EXPECT_TRUE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_Stop::ID));
}

TEST_F(BrowserPluginTest, ReloadMethod) {
  const char* kCallReload =
    "document.getElementById('browserplugin').reload();";
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  ExecuteJavaScript(kCallReload);
  EXPECT_TRUE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_Reload::ID));
}


// Verify that the 'partition' attribute on the browser plugin is parsed
// correctly.
TEST_F(BrowserPluginTest, PartitionAttribute) {
  std::string html = StringPrintf(kHTMLForPartitionedPluginObject,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  std::string partition_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("someid", partition_value.c_str());

  html = StringPrintf(kHTMLForPartitionedPersistedPluginObject,
                      content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  partition_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("persist:someid", partition_value.c_str());

  // Verify that once HTML has defined a source and partition, we cannot change
  // the partition anymore.
  ExecuteJavaScript(
      "try {"
      "  document.getElementById('browserplugin').partition = 'foo';"
      "  document.title = 'success';"
      "} catch (e) { document.title = e.message; }");
  std::string title = ExecuteScriptAndReturnString("document.title");
  EXPECT_STREQ(
      "The object has already navigated, so its partition cannot be changed.",
      title.c_str());

  // Load a browser tag without 'src' defined.
  html = StringPrintf(kHTMLForSourcelessPluginObject,
                      content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());

  // Ensure we don't parse just "persist:" string and return exception.
  ExecuteJavaScript(
      "try {"
      "  document.getElementById('browserplugin').partition = 'persist:';"
      "  document.title = 'success';"
      "} catch (e) { document.title = e.message; }");
  title = ExecuteScriptAndReturnString("document.title");
  EXPECT_STREQ("Invalid partition attribute.", title.c_str());
}

// This test verifies that BrowserPlugin enters an error state when the
// partition attribute is invalid.
TEST_F(BrowserPluginTest, InvalidPartition) {
  std::string html = StringPrintf(kHTMLForInvalidPartitionedPluginObject,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  // Attempt to navigate with an invalid partition.
  {
    ExecuteJavaScript(
        "try {"
        "  document.getElementById('browserplugin').src = 'bar';"
        "  document.title = 'success';"
        "} catch (e) { document.title = e.message; }");
    std::string title = ExecuteScriptAndReturnString("document.title");
    EXPECT_STREQ("Invalid partition attribute.", title.c_str());
    // Verify that the 'src' attribute has not been updated.
    EXPECT_EQ("", ExecuteScriptAndReturnString(
        "document.getElementById('browserplugin').src"));
  }

  // Verify that the BrowserPlugin accepts changes to its src attribue after
  // setting the partition to a valid value.
  ExecuteJavaScript(
      "document.getElementById('browserplugin').partition = 'persist:foo'");
  ExecuteJavaScript("document.getElementById('browserplugin').src = 'bar'");
  EXPECT_EQ("bar", ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').src"));
  ProcessPendingMessages();
  // Verify that the BrowserPlugin does not 'deadlock': it can recover from
  // the partition ID error state.
  {
    ExecuteJavaScript(
        "try {"
        "  document.getElementById('browserplugin').partition = 'persist:1337';"
        "  document.title = 'success';"
        "} catch (e) { document.title = e.message; }");
    std::string title = ExecuteScriptAndReturnString("document.title");
    EXPECT_STREQ(
        "The object has already navigated, so its partition cannot be changed.",
        title.c_str());
    ExecuteJavaScript("document.getElementById('browserplugin').src = '42'");
    EXPECT_EQ("42", ExecuteScriptAndReturnString(
        "document.getElementById('browserplugin').src"));
  }
}

// Test to verify that after the first navigation, the partition attribute
// cannot be modified.
TEST_F(BrowserPluginTest, ImmutableAttributesAfterNavigation) {
  std::string html = StringPrintf(kHTMLForSourcelessPluginObject,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());

  ExecuteJavaScript(
      "document.getElementById('browserplugin').partition = 'storage'");
  std::string partition_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("storage", partition_value.c_str());

  std::string src_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').src");
  EXPECT_STREQ("", src_value.c_str());

  ExecuteJavaScript("document.getElementById('browserplugin').src = 'bar'");
  ProcessPendingMessages();
  {
    const IPC::Message* create_msg =
    browser_plugin_manager()->sink().GetUniqueMessageMatching(
        BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(create_msg);

    int create_instance_id = 0;
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(
        create_msg,
        &create_instance_id,
        &params);
    EXPECT_STREQ("storage", params.storage_partition_id.c_str());
    EXPECT_FALSE(params.persist_storage);
    EXPECT_STREQ("bar", params.src.c_str());
  }

  // Setting the partition should throw an exception and the value should not
  // change.
  ExecuteJavaScript(
      "try {"
      "  document.getElementById('browserplugin').partition = 'someid';"
      "  document.title = 'success';"
      "} catch (e) { document.title = e.message; }");

  std::string title = ExecuteScriptAndReturnString("document.title");
  EXPECT_STREQ(
      "The object has already navigated, so its partition cannot be changed.",
      title.c_str());

  partition_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("storage", partition_value.c_str());
}

// This test verifies that we can mutate the event listener vector
// within an event listener.
TEST_F(BrowserPluginTest, RemoveEventListenerInEventListener) {
  const char* kAddEventListener =
    "var url;"
    "function nav(e) {"
    "  url = JSON.parse(e.detail).url;"
    "  document.getElementById('browserplugin')."
    "      removeEventListener('-internal-loadcommit', nav);"
    "}"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-loadcommit', nav);";
  const char* kGoogleURL = "http://www.google.com/";
  const char* kGoogleNewsURL = "http://news.google.com/";
  const char* kGetProcessID =
      "document.getElementById('browserplugin').getProcessId()";

  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  ExecuteJavaScript(kAddEventListener);
  // Grab the BrowserPlugin's instance ID from its CreateGuest message.
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetFirstMessageMatching(
          BrowserPluginHostMsg_CreateGuest::ID);
  ASSERT_TRUE(msg);
  int instance_id = 0;
  BrowserPluginHostMsg_CreateGuest_Params params;
  BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);

  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);

  {
    BrowserPluginMsg_LoadCommit_Params navigate_params;
    navigate_params.url = GURL(kGoogleURL);
    navigate_params.process_id = 1337;
    BrowserPluginMsg_LoadCommit msg(0, instance_id, navigate_params);
    browser_plugin->OnMessageReceived(msg);
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString("url"));
    EXPECT_EQ(1337, ExecuteScriptAndReturnInt(kGetProcessID));
  }
  {
    BrowserPluginMsg_LoadCommit_Params navigate_params;
    navigate_params.url = GURL(kGoogleNewsURL);
    navigate_params.process_id = 42;
    BrowserPluginMsg_LoadCommit msg(0, instance_id, navigate_params);
    browser_plugin->OnMessageReceived(msg);
    // The URL variable should not change because we've removed the event
    // listener.
    EXPECT_EQ(kGoogleURL, ExecuteScriptAndReturnString("url"));
    EXPECT_EQ(42, ExecuteScriptAndReturnInt(kGetProcessID));
  }
}

// This test verifies that multiple event listeners fire that are registered
// on a single event type.
TEST_F(BrowserPluginTest, MultipleEventListeners) {
  const char* kAddEventListener =
    "var count = 0;"
    "function nava(u) {"
    "  count++;"
    "}"
    "function navb(u) {"
    "  count++;"
    "}"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-loadcommit', nava);"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-loadcommit', navb);";
  const char* kGoogleURL = "http://www.google.com/";
  const char* kGetProcessID =
      "document.getElementById('browserplugin').getProcessId()";

  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  ExecuteJavaScript(kAddEventListener);
  // Grab the BrowserPlugin's instance ID from its CreateGuest message.
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetFirstMessageMatching(
          BrowserPluginHostMsg_CreateGuest::ID);
  ASSERT_TRUE(msg);
  int instance_id = 0;
  BrowserPluginHostMsg_CreateGuest_Params params;
  BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);

  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);

  {
    BrowserPluginMsg_LoadCommit_Params navigate_params;
    navigate_params.url = GURL(kGoogleURL);
    navigate_params.process_id = 1337;
    BrowserPluginMsg_LoadCommit msg(0, instance_id, navigate_params);
    browser_plugin->OnMessageReceived(msg);
    EXPECT_EQ(2, ExecuteScriptAndReturnInt("count"));
    EXPECT_EQ(1337, ExecuteScriptAndReturnInt(kGetProcessID));
  }
}

TEST_F(BrowserPluginTest, RemoveBrowserPluginOnExit) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());

  // Grab the BrowserPlugin's instance ID from its CreateGuest message.
  int instance_id = 0;
  {
    const IPC::Message* msg =
        browser_plugin_manager()->sink().GetFirstMessageMatching(
            BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(msg);
    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(msg, &instance_id, &params);
  }

  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  ASSERT_TRUE(browser_plugin);

  const char* kAddEventListener =
    "function exitListener(e) {"
    "  if (JSON.parse(e.detail).reason == 'killed') {"
    "    var bp = document.getElementById('browserplugin');"
    "    bp.parentNode.removeChild(bp);"
    "  }"
    "}"
    "document.getElementById('browserplugin')."
    "    addEventListener('-internal-exit', exitListener);";

  ExecuteJavaScript(kAddEventListener);

  // Pretend that the guest has crashed.
  BrowserPluginMsg_GuestGone msg(
      0, instance_id, 0, base::TERMINATION_STATUS_PROCESS_WAS_KILLED);
  browser_plugin->OnMessageReceived(msg);

  ProcessPendingMessages();

  EXPECT_EQ(NULL, browser_plugin_manager()->GetBrowserPlugin(instance_id));
}

TEST_F(BrowserPluginTest, AutoSizeAttributes) {
  std::string html = StringPrintf(kHTMLForSourcelessPluginObject,
                                  content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  const char* kSetAutoSizeParametersAndNavigate =
    "var browserplugin = document.getElementById('browserplugin');"
    "browserplugin.autosize = true;"
    "browserplugin.minwidth = 42;"
    "browserplugin.minheight = 43;"
    "browserplugin.maxwidth = 1337;"
    "browserplugin.maxheight = 1338;"
    "browserplugin.src = 'foobar';";
  const char* kDisableAutoSize =
    "document.getElementById('browserplugin').removeAttribute('autosize');";

  int instance_id = 0;
  // Set some autosize parameters before navigating then navigate.
  // Verify that the BrowserPluginHostMsg_CreateGuest message contains
  // the correct autosize parameters.
  ExecuteJavaScript(kSetAutoSizeParametersAndNavigate);
  ProcessPendingMessages();
  {
    const IPC::Message* create_msg =
    browser_plugin_manager()->sink().GetUniqueMessageMatching(
        BrowserPluginHostMsg_CreateGuest::ID);
    ASSERT_TRUE(create_msg);

    BrowserPluginHostMsg_CreateGuest_Params params;
    BrowserPluginHostMsg_CreateGuest::Read(
        create_msg,
        &instance_id,
        &params);
     EXPECT_TRUE(params.auto_size_params.enable);
     EXPECT_EQ(42, params.auto_size_params.min_size.width());
     EXPECT_EQ(43, params.auto_size_params.min_size.height());
     EXPECT_EQ(1337, params.auto_size_params.max_size.width());
     EXPECT_EQ(1338, params.auto_size_params.max_size.height());
  }
  // Verify that we are waiting for the browser process to grab the new
  // damage buffer.
  MockBrowserPlugin* browser_plugin =
      static_cast<MockBrowserPlugin*>(
          browser_plugin_manager()->GetBrowserPlugin(instance_id));
  EXPECT_TRUE(browser_plugin->pending_damage_buffer_.get());
  // Disable autosize. AutoSize state will not be sent to the guest until
  // the guest has responded to the last resize request.
  ExecuteJavaScript(kDisableAutoSize);
  ProcessPendingMessages();

  const IPC::Message* auto_size_msg =
  browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_SetAutoSize::ID);
  EXPECT_FALSE(auto_size_msg);

  // Send the BrowserPlugin an UpdateRect equal to its |max_size| with
  // the same damage buffer.
  BrowserPluginMsg_UpdateRect_Params update_rect_params;
  update_rect_params.damage_buffer_sequence_id =
      browser_plugin->damage_buffer_sequence_id_;
  update_rect_params.view_size = gfx::Size(1337, 1338);
  update_rect_params.scale_factor = 1.0f;
  update_rect_params.is_resize_ack = true;
  update_rect_params.needs_ack = true;
  BrowserPluginMsg_UpdateRect msg(0, instance_id, update_rect_params);
  browser_plugin->OnMessageReceived(msg);

  // Verify that the autosize state has been updated.
  {
    const IPC::Message* auto_size_msg =
    browser_plugin_manager()->sink().GetUniqueMessageMatching(
        BrowserPluginHostMsg_UpdateRect_ACK::ID);
    ASSERT_TRUE(auto_size_msg);

    int instance_id = 0;
    BrowserPluginHostMsg_AutoSize_Params auto_size_params;
    BrowserPluginHostMsg_ResizeGuest_Params resize_params;
    BrowserPluginHostMsg_UpdateRect_ACK::Read(auto_size_msg,
                                              &instance_id,
                                              &auto_size_params,
                                              &resize_params);
    EXPECT_FALSE(auto_size_params.enable);
    // These value are not populated (as an optimization) if autosize is
    // disabled.
    EXPECT_EQ(0, auto_size_params.min_size.width());
    EXPECT_EQ(0, auto_size_params.min_size.height());
    EXPECT_EQ(0, auto_size_params.max_size.width());
    EXPECT_EQ(0, auto_size_params.max_size.height());
  }
}

}  // namespace content
