// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/extension_process_bindings.h"
#include "chrome/renderer/extensions/renderer_extension_bindings.h"
#include "chrome/test/render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class ExtensionAPIClientTest : public RenderViewTest {
 protected:
  virtual void SetUp() {
    RenderViewTest::SetUp();

    render_thread_.sink().ClearMessages();
    LoadHTML("<body></body>");
  }

  std::string GetConsoleMessage() {
    const IPC::Message* message =
        render_thread_.sink().GetUniqueMessageMatching(
            ViewHostMsg_AddMessageToConsole::ID);
    ViewHostMsg_AddMessageToConsole::Param params;
    if (message) {
      ViewHostMsg_AddMessageToConsole::Read(message, &params);
      render_thread_.sink().ClearMessages();
      return WideToASCII(params.a);
    } else {
      return "";
    }
  }

  void ExpectJsFail(const std::string& js, const std::string& message) {
    ExecuteJavaScript(js.c_str());
    EXPECT_EQ(message, GetConsoleMessage());
    render_thread_.sink().ClearMessages();
  }

  void ExpectJsPass(const std::string& js,
                    const std::string& function,
                    const std::string& arg1) {
    ExecuteJavaScript(js.c_str());
    const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
    ASSERT_EQ("", GetConsoleMessage()) << js;
    ASSERT_TRUE(request_msg) << js;
    ViewHostMsg_ExtensionRequest::Param params;
    ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
    ASSERT_EQ(function.c_str(), params.a) << js;

    Value* args = NULL;
    ASSERT_TRUE(params.b.IsType(Value::TYPE_LIST));
    ASSERT_TRUE(static_cast<const ListValue*>(&params.b)->Get(0, &args));

    base::JSONReader reader;
    scoped_ptr<Value> arg1_value(reader.JsonToValue(arg1, false, false));
    ASSERT_TRUE(args->Equals(arg1_value.get())) << js;
    render_thread_.sink().ClearMessages();
  }
};

// Tests that callback dispatching works correctly and that JSON is properly
// deserialized before handing off to the extension code. We use the createTab
// API here, but we could use any of them since they all dispatch callbacks the
// same way.
TEST_F(ExtensionAPIClientTest, CallbackDispatching) {
  ExecuteJavaScript(
    "function assert(truth, message) {"
    "  if (!truth) {"
    "    throw new Error(message);"
    "  }"
    "}"
    "function callback(result) {"
    "  assert(typeof result == 'object', 'result not object');"
    "  assert(JSON.stringify(result) == '{\"id\":1,\"index\":1,\"windowId\":1,"
                                        "\"selected\":true,"
                                        "\"url\":\"http://www.google.com/\"}',"
    "         'incorrect result');"
    "  console.log('pass')"
    "}"
    "chrome.tabs.create({}, callback);"
  );

  EXPECT_EQ("", GetConsoleMessage());

  // Ok, we should have gotten a message to create a tab, grab the callback ID.
  const IPC::Message* request_msg =
      render_thread_.sink().GetUniqueMessageMatching(
          ViewHostMsg_ExtensionRequest::ID);
  ASSERT_TRUE(request_msg);
  ViewHostMsg_ExtensionRequest::Param params;
  ViewHostMsg_ExtensionRequest::Read(request_msg, &params);
  int callback_id = params.c;
  ASSERT_GE(callback_id, 0);

  // Now send the callback a response
  ExtensionProcessBindings::HandleResponse(
    callback_id, true, "{\"id\":1,\"index\":1,\"windowId\":1,\"selected\":true,"
                       "\"url\":\"http://www.google.com/\"}", "");

  // And verify that it worked
  ASSERT_EQ("pass", GetConsoleMessage());
}

// The remainder of these tests exercise the client side of the various
// extension functions. We test both error and success conditions, but do not
// test errors exhaustively as json schema code is well tested by itself.

// Window API tests

TEST_F(ExtensionAPIClientTest, GetWindow) {
  ExpectJsFail("chrome.windows.get(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.windows.get(32);",
               "Uncaught Error: Parameter 1 is required.");

  ExpectJsFail("chrome.windows.get('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.windows.get(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.windows.get(2, function(){})",
               "windows.get", "2");
}

TEST_F(ExtensionAPIClientTest, GetCurentWindow) {
  ExpectJsFail("chrome.windows.getCurrent(function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.windows.getCurrent();",
               "Uncaught Error: Parameter 0 is required.");

  ExpectJsFail("chrome.windows.getCurrent('abc');",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'function' but got 'string'.");

  ExpectJsPass("chrome.windows.getCurrent(function(){})",
               "windows.getCurrent", "null");
}

// http://crbug.com/22248
TEST_F(ExtensionAPIClientTest, DISABLED_GetLastFocusedWindow) {
  ExpectJsFail("chrome.windows.getLastFocused(function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.windows.getLastFocused();",
               "Uncaught Error: Parameter 0 is required.");

  ExpectJsFail("chrome.windows.getLastFocused('abc');",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'function' but got 'string'.");

  ExpectJsPass("chrome.windows.getLastFocused(function(){})",
               "windows.getLastFocused", "null");
}

TEST_F(ExtensionAPIClientTest, GetAllWindows) {
  ExpectJsFail("chrome.windows.getAll({populate: true}, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.windows.getAll(1, function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'object' but got 'integer'.");

  ExpectJsPass("chrome.windows.getAll({populate:true}, function(){})",
               "windows.getAll", "{\"populate\":true}");

  ExpectJsPass("chrome.windows.getAll(null, function(){})",
               "windows.getAll", "null");

  ExpectJsPass("chrome.windows.getAll({}, function(){})",
               "windows.getAll", "{}");

  ExpectJsPass("chrome.windows.getAll(undefined, function(){})",
               "windows.getAll", "null");
}

TEST_F(ExtensionAPIClientTest, CreateWindow) {
  ExpectJsFail("chrome.windows.create({url: 1}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");
  ExpectJsFail("chrome.windows.create({left: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'left': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.create({top: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'top': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.create({width: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'width': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.create({height: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'height': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.create({foo: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'foo': Unexpected property.");

  ExpectJsPass("chrome.windows.create({"
               "  url:'http://www.google.com/',"
               "  left:0,"
               "  top: 10,"
               "  width:100,"
               "  height:200"
               "})",
               "windows.create",
               "{\"url\":\"http://www.google.com/\","
               "\"left\":0,"
               "\"top\":10,"
               "\"width\":100,"
               "\"height\":200}");
}

TEST_F(ExtensionAPIClientTest, UpdateWindow) {
  ExpectJsFail("chrome.windows.update(null);",
               "Uncaught Error: Parameter 0 is required.");
  ExpectJsFail("chrome.windows.update(42, {left: 'foo'});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'left': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.update(42, {top: 'foo'});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'top': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.windows.update(42, {height: false});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'height': Expected 'integer' but got 'boolean'.");
  ExpectJsFail("chrome.windows.update(42, {width: false});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'width': Expected 'integer' but got 'boolean'.");
  ExpectJsFail("chrome.windows.update(42, {foo: false});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'foo': Unexpected property.");

  ExpectJsPass("chrome.windows.update(42, {"
               "  width:100,"
               "  height:200"
               "})",
               "windows.update",
               "[42,"
               "{\"width\":100,"
               "\"height\":200}]");
}

TEST_F(ExtensionAPIClientTest, RemoveWindow) {
  ExpectJsFail("chrome.windows.remove(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.windows.remove('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.windows.remove(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.windows.remove(2, function(){})",
               "windows.remove", "2");

  ExpectJsPass("chrome.windows.remove(2)",
               "windows.remove", "2");
}

// Tab API tests

TEST_F(ExtensionAPIClientTest, GetTab) {
  ExpectJsFail("chrome.tabs.get(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.tabs.get(32);",
               "Uncaught Error: Parameter 1 is required.");

  ExpectJsFail("chrome.tabs.get('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.tabs.get(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.tabs.get(2, function(){})",
               "tabs.get", "2");
}

#if defined(OS_WIN)
TEST_F(ExtensionAPIClientTest, DetectTabLanguage) {
  ExpectJsFail("chrome.tabs.detectLanguage(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.tabs.detectLanguage('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.tabs.detectLanguage(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.tabs.detectLanguage(null, function(){})",
               "tabs.detectLanguage", "null");
}
#endif

TEST_F(ExtensionAPIClientTest, GetSelectedTab) {
  ExpectJsFail("chrome.tabs.getSelected(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.tabs.getSelected(32);",
               "Uncaught Error: Parameter 1 is required.");

  ExpectJsFail("chrome.tabs.getSelected('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.tabs.getSelected(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.tabs.getSelected(2, function(){})",
               "tabs.getSelected", "2");

  ExpectJsPass("chrome.tabs.getSelected(null, function(){})",
               "tabs.getSelected", "null");
}


TEST_F(ExtensionAPIClientTest, GetAllTabsInWindow) {
  ExpectJsFail("chrome.tabs.getAllInWindow(42, function(){}, 'asd');",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.tabs.getAllInWindow(32);",
               "Uncaught Error: Parameter 1 is required.");

  ExpectJsFail("chrome.tabs.getAllInWindow(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsFail("chrome.tabs.getAllInWindow('asd', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsPass("chrome.tabs.getAllInWindow(32, function(){})",
               "tabs.getAllInWindow", "32");

  ExpectJsPass("chrome.tabs.getAllInWindow(undefined, function(){})",
               "tabs.getAllInWindow", "null");
}

TEST_F(ExtensionAPIClientTest, CreateTab) {
  ExpectJsFail("chrome.tabs.create({windowId: 'foo'}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'windowId': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.tabs.create({url: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'url': Expected 'string' but got 'integer'.");
  ExpectJsFail("chrome.tabs.create({foo: 42}, function(){});",
               "Uncaught Error: Invalid value for argument 0. Property "
               "'foo': Unexpected property.");

  ExpectJsPass("chrome.tabs.create({"
               "  url:'http://www.google.com/',"
               "  selected:true,"
               "  index: 2,"
               "  windowId:4"
               "})",
               "tabs.create",
               "{\"url\":\"http://www.google.com/\","
               "\"selected\":true,"
               "\"index\":2,"
               "\"windowId\":4}");
}

TEST_F(ExtensionAPIClientTest, UpdateTab) {
  ExpectJsFail("chrome.tabs.update(null);",
               "Uncaught Error: Parameter 0 is required.");
  ExpectJsFail("chrome.tabs.update(42, {selected: 'foo'});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'selected': Expected 'boolean' but got 'string'.");
  ExpectJsFail("chrome.tabs.update(42, {url: 42});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'url': Expected 'string' but got 'integer'.");

  ExpectJsPass("chrome.tabs.update(42, {"
               "  url:'http://www.google.com/',"
               "  selected:true"
               "})",
               "tabs.update",
               "[42,"
               "{\"url\":\"http://www.google.com/\","
               "\"selected\":true}]");
}

TEST_F(ExtensionAPIClientTest, MoveTab) {
  ExpectJsFail("chrome.tabs.move(null);",
               "Uncaught Error: Parameter 0 is required.");
  ExpectJsFail("chrome.tabs.move(42, {index: 'foo'});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'index': Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.tabs.move(42, {index: 3, windowId: 'foo'});",
               "Uncaught Error: Invalid value for argument 1. Property "
               "'windowId': Expected 'integer' but got 'string'.");

  ExpectJsPass("chrome.tabs.move(42, {"
               "  index:3,"
               "  windowId:21"
               "})",
               "tabs.move",
               "[42,"
               "{\"index\":3,"
               "\"windowId\":21}]");
}

TEST_F(ExtensionAPIClientTest, RemoveTab) {
  ExpectJsFail("chrome.tabs.remove(32, function(){}, 20);",
               "Uncaught Error: Too many arguments.");


  ExpectJsFail("chrome.tabs.remove('abc', function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'string'.");

  ExpectJsFail("chrome.tabs.remove(1, 1);",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");

  ExpectJsPass("chrome.tabs.remove(2, function(){})",
               "tabs.remove", "2");

  ExpectJsPass("chrome.tabs.remove(2)",
               "tabs.remove", "2");
}

TEST_F(ExtensionAPIClientTest, CaptureVisibleTab) {
  ExpectJsFail("chrome.tabs.captureVisibleTab(0);",
               "Uncaught Error: Parameter 1 is required.");

  ExpectJsFail("chrome.tabs.captureVisibleTab(function(){}, 0)",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'integer' but got 'function'.");

  ExpectJsPass("chrome.tabs.captureVisibleTab(null, function(img){});",
               "tabs.captureVisibleTab", "null");
}

// Bookmark API tests
// TODO(erikkay) add more variations here

TEST_F(ExtensionAPIClientTest, CreateBookmark) {
  ExpectJsFail(
      "chrome.bookmarks.create({parentId:0, title:0}, function(){})",
      "Uncaught Error: Invalid value for argument 0. "
      "Property 'parentId': Expected 'string' but got 'integer', "
      "Property 'title': Expected 'string' but got 'integer'.");

  ExpectJsPass(
      "chrome.bookmarks.create({parentId:'0', title:'x'}, function(){})",
      "bookmarks.create",
      "{\"parentId\":\"0\",\"title\":\"x\"}");
}

TEST_F(ExtensionAPIClientTest, GetBookmarks) {
  ExpectJsPass("chrome.bookmarks.get('0', function(){});",
               "bookmarks.get",
               "\"0\"");
  ExpectJsPass("chrome.bookmarks.get(['0','1','2','3'], function(){});",
               "bookmarks.get",
               "[\"0\",\"1\",\"2\",\"3\"]");
  ExpectJsFail("chrome.bookmarks.get(null, function(){});",
               "Uncaught Error: Parameter 0 is required.");
  // TODO(erikkay) This is succeeding, when it should fail.
  // BUG=13719
#if 0
  ExpectJsFail("chrome.bookmarks.get({}, function(){});",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'array' but got 'object'.");
#endif
}

TEST_F(ExtensionAPIClientTest, GetBookmarkChildren) {
  ExpectJsPass("chrome.bookmarks.getChildren('42', function(){});",
               "bookmarks.getChildren",
               "\"42\"");
}

TEST_F(ExtensionAPIClientTest, GetBookmarkRecent) {
  ExpectJsPass("chrome.bookmarks.getRecent(5, function(){});",
    "bookmarks.getRecent",
    "5");
}

TEST_F(ExtensionAPIClientTest, GetBookmarkTree) {
  ExpectJsPass("chrome.bookmarks.getTree(function(){});",
               "bookmarks.getTree",
               "null");
}

TEST_F(ExtensionAPIClientTest, SearchBookmarks) {
  ExpectJsPass("chrome.bookmarks.search('hello',function(){});",
               "bookmarks.search",
               "\"hello\"");
}

TEST_F(ExtensionAPIClientTest, RemoveBookmark) {
  ExpectJsPass("chrome.bookmarks.remove('42');",
               "bookmarks.remove",
               "\"42\"");
}

TEST_F(ExtensionAPIClientTest, RemoveBookmarkTree) {
  ExpectJsPass("chrome.bookmarks.removeTree('42');",
               "bookmarks.removeTree",
               "\"42\"");
}

TEST_F(ExtensionAPIClientTest, MoveBookmark) {
  ExpectJsPass("chrome.bookmarks.move('42',{parentId:'1',index:0});",
               "bookmarks.move",
               "[\"42\",{\"parentId\":\"1\",\"index\":0}]");
}

TEST_F(ExtensionAPIClientTest, SetBookmarkTitle) {
  ExpectJsPass("chrome.bookmarks.update('42',{title:'x'});",
               "bookmarks.update",
               "[\"42\",{\"title\":\"x\"}]");
}

TEST_F(ExtensionAPIClientTest, EnablePageAction) {
  // Basic old-school enablePageAction call.
  ExpectJsPass("chrome.pageActions.enableForTab("
               "'dummy', {tabId: 0, url: 'http://foo/'});",
               "pageActions.enableForTab",
               "[\"dummy\",{\"tabId\":0,\"url\":\"http://foo/\"}]");
  // Try both optional parameters (title and iconId).
  ExpectJsPass("chrome.pageActions.enableForTab("
               "'dummy', {tabId: 0, url: 'http://foo/',"
                           "title: 'a', iconId: 0});",
               "pageActions.enableForTab",
               "[\"dummy\",{\"tabId\":0,\"url\":\"http://foo/\","
                           "\"title\":\"a\",\"iconId\":0}]");

  // Now try disablePageAction.
  ExpectJsPass("chrome.pageActions.disableForTab("
               "'dummy', {tabId: 0, url: 'http://foo/'});",
               "pageActions.disableForTab",
               "[\"dummy\",{\"tabId\":0,\"url\":\"http://foo/\"}]");
}

TEST_F(ExtensionAPIClientTest, ExpandToolstrip) {
  ExpectJsPass("chrome.toolstrip.expand({height:100, url:'http://foo/'})",
               "toolstrip.expand",
               "{\"height\":100,\"url\":\"http://foo/\"}");
  ExpectJsPass("chrome.toolstrip.expand({height:100}, null)",
               "toolstrip.expand",
               "{\"height\":100}");
  ExpectJsPass("chrome.toolstrip.expand({height:100,url:'http://foo/'}, "
                   "function(){})",
               "toolstrip.expand",
               "{\"height\":100,\"url\":\"http://foo/\"}");


  ExpectJsFail("chrome.toolstrip.expand()",
               "Uncaught Error: Parameter 0 is required.");
  ExpectJsFail("chrome.toolstrip.expand(1)",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'object' but got 'integer'.");
  ExpectJsFail("chrome.toolstrip.expand({height:'100', url:'http://foo/'})",
               "Uncaught Error: Invalid value for argument 0. "
                   "Property 'height': "
               "Expected 'integer' but got 'string'.");
  ExpectJsFail("chrome.toolstrip.expand({height:100,url:100})",
               "Uncaught Error: Invalid value for argument 0. Property 'url': "
               "Expected 'string' but got 'integer'.");
  ExpectJsFail("chrome.toolstrip.expand({height:100,'url':'http://foo/'}, 32)",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");
}

TEST_F(ExtensionAPIClientTest, CollapseToolstrip) {
  ExpectJsPass("chrome.toolstrip.collapse({url:'http://foo/'})",
               "toolstrip.collapse",
               "{\"url\":\"http://foo/\"}");
  ExpectJsPass("chrome.toolstrip.collapse(null)",
               "toolstrip.collapse",
               "null");
  ExpectJsPass("chrome.toolstrip.collapse({url:'http://foo/'}, function(){})",
               "toolstrip.collapse",
               "{\"url\":\"http://foo/\"}");

  ExpectJsFail("chrome.toolstrip.collapse(1)",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'object' but got 'integer'.");
  ExpectJsFail("chrome.toolstrip.collapse(100)",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'object' but got 'integer'.");
  ExpectJsFail("chrome.toolstrip.collapse({url:'http://foo/'}, 32)",
               "Uncaught Error: Invalid value for argument 1. "
               "Expected 'function' but got 'integer'.");
}

// I18N API
TEST_F(ExtensionAPIClientTest, GetAcceptLanguages) {
  ExpectJsFail("chrome.i18n.getAcceptLanguages(32, function(){})",
               "Uncaught Error: Too many arguments.");

  ExpectJsFail("chrome.i18n.getAcceptLanguages()",
               "Uncaught Error: Parameter 0 is required.");

  ExpectJsFail("chrome.i18n.getAcceptLanguages('abc')",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'function' but got 'string'.");

  ExpectJsPass("chrome.i18n.getAcceptLanguages(function(){})",
               "i18n.getAcceptLanguages", "null");
}

TEST_F(ExtensionAPIClientTest, GetL10nMessage) {
  ExpectJsFail("chrome.i18n.getMessage()",
               "Uncaught Error: Parameter 0 is required.");

  ExpectJsFail("chrome.i18n.getMessage(1)",
               "Uncaught Error: Invalid value for argument 0. "
               "Expected 'string' but got 'integer'.");

  ExpectJsFail("chrome.i18n.getMessage('name', [])",
               "Uncaught Error: Invalid value for argument 1. Value does not "
               "match any valid type choices.");

  ExpectJsFail("chrome.i18n.getMessage('name', ['p1', 'p2', 'p3', 'p4', 'p5', "
               "'p6', 'p7', 'p8', 'p9', 'p10'])",
               "Uncaught Error: Invalid value for argument 1. Value does not "
               "match any valid type choices.");
}
