// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_browsertest.h"

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "content/public/common/content_constants.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_factory.h"
#include "content/renderer/browser_plugin/mock_browser_plugin.h"
#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/web/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

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
  return base::StringPrintf(kHTMLForBrowserPluginObject,
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
  virtual bool AllowBrowserPlugin(
      WebKit::WebPluginContainer* container) OVERRIDE {
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
  SetRendererClientForTesting(test_content_renderer_client_.get());
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
  scoped_ptr<char[]> str(new char[length]);
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

MockBrowserPlugin* BrowserPluginTest::GetCurrentPlugin() {
  BrowserPluginHostMsg_Attach_Params params;
  return GetCurrentPluginWithAttachParams(&params);
}

MockBrowserPlugin* BrowserPluginTest::GetCurrentPluginWithAttachParams(
    BrowserPluginHostMsg_Attach_Params* params) {
  int instance_id = 0;
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetUniqueMessageMatching(
          BrowserPluginHostMsg_Attach::ID);
  if (!msg)
    return NULL;

  PickleIterator iter(*msg);
  if (!iter.ReadInt(&instance_id))
    return NULL;

  if (!IPC::ParamTraits<BrowserPluginHostMsg_Attach_Params>::Read(
      msg, &iter, params))
    return NULL;

  return static_cast<MockBrowserPlugin*>(
      browser_plugin_manager()->GetBrowserPlugin(instance_id));
}

// This test verifies that an initial resize occurs when we instantiate the
// browser plugin. This test also verifies that the browser plugin is waiting
// for a BrowserPluginMsg_UpdateRect in response. We issue an UpdateRect, and
// we observe an UpdateRect_ACK, with the |pending_damage_buffer_| reset,
// indiciating that the BrowserPlugin is not waiting for any more UpdateRects to
// satisfy its resize request.
TEST_F(BrowserPluginTest, InitialResize) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  // Verify that the information in Attach is correct.
  BrowserPluginHostMsg_Attach_Params params;
  MockBrowserPlugin* browser_plugin = GetCurrentPluginWithAttachParams(&params);

  EXPECT_EQ(640, params.resize_guest_params.view_rect.width());
  EXPECT_EQ(480, params.resize_guest_params.view_rect.height());
  ASSERT_TRUE(browser_plugin);
  // Now the browser plugin is expecting a UpdateRect resize.
  int instance_id = browser_plugin->guest_instance_id();
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
  BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
  browser_plugin->OnMessageReceived(msg);
  EXPECT_FALSE(browser_plugin->pending_damage_buffer_.get());
}

// This test verifies that all attributes (present at the time of writing) are
// parsed on initialization. However, this test does minimal checking of
// correct behavior.
TEST_F(BrowserPluginTest, ParseAllAttributes) {
  std::string html = base::StringPrintf(kHTMLForBrowserPluginWithAllAttributes,
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
    BrowserPluginHostMsg_Attach_Params params;
    MockBrowserPlugin* browser_plugin =
        GetCurrentPluginWithAttachParams(&params);
    ASSERT_TRUE(browser_plugin);
    EXPECT_EQ("foo", params.src);
  }

  browser_plugin_manager()->sink().ClearMessages();
  // Navigate to bar and observe the associated
  // BrowserPluginHostMsg_NavigateGuest message.
  // Verify that the src attribute is updated as well.
  ExecuteJavaScript("document.getElementById('browserplugin').src = 'bar'");
  {
    // Verify that we do not get a Attach on subsequent navigations.
    const IPC::Message* create_msg =
        browser_plugin_manager()->sink().GetUniqueMessageMatching(
            BrowserPluginHostMsg_Attach::ID);
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
  MockBrowserPlugin* browser_plugin = GetCurrentPlugin();
  ASSERT_TRUE(browser_plugin);
  int instance_id = browser_plugin->guest_instance_id();
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
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
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

  // Expect to see one resize messsage in the sink. BrowserPlugin will not issue
  // subsequent resize requests until the first request is satisfied by the
  // guest. The rest of the messages could be
  // BrowserPluginHostMsg_UpdateGeometry msgs.
  EXPECT_LE(1u, browser_plugin_manager()->sink().message_count());
  for (size_t i = 0; i < browser_plugin_manager()->sink().message_count();
       ++i) {
    const IPC::Message* msg = browser_plugin_manager()->sink().GetMessageAt(i);
    if (msg->type() != BrowserPluginHostMsg_ResizeGuest::ID)
      EXPECT_EQ(msg->type(), BrowserPluginHostMsg_UpdateGeometry::ID);
  }
  const IPC::Message* msg =
      browser_plugin_manager()->sink().GetUniqueMessageMatching(
          BrowserPluginHostMsg_ResizeGuest::ID);
  ASSERT_TRUE(msg);
  BrowserPluginHostMsg_ResizeGuest_Params params;
  BrowserPluginHostMsg_ResizeGuest::Read(msg, &instance_id, &params);
  EXPECT_EQ(641, params.view_rect.width());
  EXPECT_EQ(480, params.view_rect.height());
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
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
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
    BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
    browser_plugin->OnMessageReceived(msg);
    // The BrowserPlugin has finally received an UpdateRect that satisifes
    // its current size, and so it is happy.
    EXPECT_FALSE(browser_plugin->pending_damage_buffer_.get());
  }
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
  std::string html = base::StringPrintf(kHTMLForSourcelessPluginObject,
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

// Verify that the 'partition' attribute on the browser plugin is parsed
// correctly.
TEST_F(BrowserPluginTest, PartitionAttribute) {
  std::string html = base::StringPrintf(kHTMLForPartitionedPluginObject,
                                        content::kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  std::string partition_value = ExecuteScriptAndReturnString(
      "document.getElementById('browserplugin').partition");
  EXPECT_STREQ("someid", partition_value.c_str());

  html = base::StringPrintf(kHTMLForPartitionedPersistedPluginObject,
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
  html = base::StringPrintf(kHTMLForSourcelessPluginObject,
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
  std::string html = base::StringPrintf(kHTMLForInvalidPartitionedPluginObject,
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
  std::string html = base::StringPrintf(kHTMLForSourcelessPluginObject,
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
    BrowserPluginHostMsg_Attach_Params params;
    MockBrowserPlugin* browser_plugin =
        GetCurrentPluginWithAttachParams(&params);
    ASSERT_TRUE(browser_plugin);

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

TEST_F(BrowserPluginTest, AutoSizeAttributes) {
  std::string html = base::StringPrintf(kHTMLForSourcelessPluginObject,
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
  // Verify that the BrowserPluginHostMsg_Attach message contains
  // the correct autosize parameters.
  ExecuteJavaScript(kSetAutoSizeParametersAndNavigate);
  ProcessPendingMessages();

  BrowserPluginHostMsg_Attach_Params params;
  MockBrowserPlugin* browser_plugin =
      GetCurrentPluginWithAttachParams(&params);
  ASSERT_TRUE(browser_plugin);

  EXPECT_TRUE(params.auto_size_params.enable);
  EXPECT_EQ(42, params.auto_size_params.min_size.width());
  EXPECT_EQ(43, params.auto_size_params.min_size.height());
  EXPECT_EQ(1337, params.auto_size_params.max_size.width());
  EXPECT_EQ(1338, params.auto_size_params.max_size.height());

  // Verify that we are waiting for the browser process to grab the new
  // damage buffer.
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
  BrowserPluginMsg_UpdateRect msg(instance_id, update_rect_params);
  browser_plugin->OnMessageReceived(msg);

  // Verify that the autosize state has been updated.
  {
    const IPC::Message* auto_size_msg =
    browser_plugin_manager()->sink().GetUniqueMessageMatching(
        BrowserPluginHostMsg_UpdateRect_ACK::ID);
    ASSERT_TRUE(auto_size_msg);

    int instance_id = 0;
    bool needs_ack = false;
    BrowserPluginHostMsg_AutoSize_Params auto_size_params;
    BrowserPluginHostMsg_ResizeGuest_Params resize_params;
    BrowserPluginHostMsg_UpdateRect_ACK::Read(auto_size_msg,
                                              &instance_id,
                                              &needs_ack,
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
