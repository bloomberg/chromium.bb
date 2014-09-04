// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_browsertest.h"

#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager_factory.h"
#include "content/renderer/browser_plugin/mock_browser_plugin.h"
#include "content/renderer/browser_plugin/mock_browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebCursorInfo.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"

namespace content {

namespace {
const char kHTMLForBrowserPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px'"
    " src='foo' type='%s'></object>"
    "<script>document.querySelector('object').nonExistentAttribute;</script>";

const char kHTMLForSourcelessPluginObject[] =
    "<object id='browserplugin' width='640px' height='480px' type='%s'>";

std::string GetHTMLForBrowserPluginObject() {
  return base::StringPrintf(kHTMLForBrowserPluginObject,
                            kBrowserPluginMimeType);
}

}  // namespace

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
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
  content::RenderViewTest::SetUp();
}

void BrowserPluginTest::TearDown() {
  BrowserPluginManager::set_factory_for_testing(
      TestBrowserPluginManagerFactory::GetInstance());
#if defined(LEAK_SANITIZER)
  // Do this before shutting down V8 in RenderViewTest::TearDown().
  // http://crbug.com/328552
  __lsan_do_leak_check();
#endif
  RenderViewTest::TearDown();
}

std::string BrowserPluginTest::ExecuteScriptAndReturnString(
    const std::string& script) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
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
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
  if (value.IsEmpty() || !value->IsInt32())
    return 0;

  return value->Int32Value();
}

// A return value of false means that a value was not present. The return value
// of the script is stored in |result|
bool BrowserPluginTest::ExecuteScriptAndReturnBool(
    const std::string& script, bool* result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::Value> value = GetMainFrame()->executeScriptAndReturnValue(
      blink::WebScriptSource(blink::WebString::fromUTF8(script.c_str())));
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
  MockBrowserPlugin* browser_plugin = static_cast<MockBrowserPluginManager*>(
      browser_plugin_manager())->last_plugin();
  if (!browser_plugin)
    return NULL;

  browser_plugin->Attach();

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
      msg, &iter, params)) {
    return NULL;
  }

  browser_plugin->OnAttachACK(instance_id);
  return browser_plugin;
}

// This test verifies that an initial resize occurs when we instantiate the
// browser plugin.
TEST_F(BrowserPluginTest, InitialResize) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  // Verify that the information in Attach is correct.
  BrowserPluginHostMsg_Attach_Params params;
  MockBrowserPlugin* browser_plugin = GetCurrentPluginWithAttachParams(&params);

  EXPECT_EQ(640, params.resize_guest_params.view_size.width());
  EXPECT_EQ(480, params.resize_guest_params.view_size.height());
  ASSERT_TRUE(browser_plugin);
}

TEST_F(BrowserPluginTest, RemovePlugin) {
  LoadHTML(GetHTMLForBrowserPluginObject().c_str());
  MockBrowserPlugin* browser_plugin = GetCurrentPlugin();
  ASSERT_TRUE(browser_plugin);

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
                                        kBrowserPluginMimeType);
  LoadHTML(html.c_str());
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
  ExecuteJavaScript("x = document.getElementById('browserplugin'); "
                    "x.parentNode.removeChild(x);");
  ProcessPendingMessages();
  EXPECT_FALSE(browser_plugin_manager()->sink().GetUniqueMessageMatching(
      BrowserPluginHostMsg_PluginDestroyed::ID));
}

}  // namespace content
