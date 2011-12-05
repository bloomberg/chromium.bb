// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_message_service.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/extensions/chrome_v8_context.h"
#include "chrome/renderer/extensions/extension_dispatcher.h"
#include "chrome/renderer/extensions/miscellaneous_bindings.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::MiscellaneousBindings;

namespace {

static const char kTestingExtensionId[] = "oooooooooooooooooooooooooooooooo";

void DispatchOnConnect(const ChromeV8ContextSet& v8_context_set,
                       int source_port_id, const std::string& name,
                       const std::string& tab_json) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  args.Set(1, Value::CreateStringValue(name));
  args.Set(2, Value::CreateStringValue(tab_json));
  args.Set(3, Value::CreateStringValue(kTestingExtensionId));
  args.Set(4, Value::CreateStringValue(kTestingExtensionId));
  v8_context_set.DispatchChromeHiddenMethod(
      "", ExtensionMessageService::kDispatchOnConnect, args, NULL, GURL());
}

void DispatchOnDisconnect(const ChromeV8ContextSet& v8_context_set,
                          int source_port_id) {
  ListValue args;
  args.Set(0, Value::CreateIntegerValue(source_port_id));
  v8_context_set.DispatchChromeHiddenMethod(
      "", ExtensionMessageService::kDispatchOnDisconnect, args, NULL, GURL());
}

}

// Tests that the bindings for opening a channel to an extension and sending
// and receiving messages through that channel all works.
//
// TODO(aa): Refactor MiscellaneousBindings to have fewer dependencies and
// make this into a unit test. That will allow us to get rid of cruft like
// SetTestExtensionId().
TEST_F(ChromeRenderViewTest, ExtensionMessagesOpenChannel) {
  extension_dispatcher_->SetTestExtensionId(kTestingExtensionId);
  render_thread_->sink().ClearMessages();
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "var port = chrome.extension.connect({name:'testName'});"
    "port.onMessage.addListener(doOnMessage);"
    "port.postMessage({message: 'content ready'});"
    "function doOnMessage(msg, port) {"
    "  alert('content got: ' + msg.val);"
    "}");

  // Verify that we opened a channel and sent a message through it.
  const IPC::Message* open_channel_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          ExtensionHostMsg_OpenChannelToExtension::ID);
  ASSERT_TRUE(open_channel_msg);
  void* iter = IPC::SyncMessage::GetDataIterator(open_channel_msg);
  ExtensionHostMsg_OpenChannelToExtension::SendParam open_params;
  ASSERT_TRUE(IPC::ReadParam(open_channel_msg, &iter, &open_params));
  EXPECT_EQ("testName", open_params.d);

  const IPC::Message* post_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          ExtensionHostMsg_PostMessage::ID);
  ASSERT_TRUE(post_msg);
  ExtensionHostMsg_PostMessage::Param post_params;
  ExtensionHostMsg_PostMessage::Read(post_msg, &post_params);
  EXPECT_EQ("{\"message\":\"content ready\"}", post_params.b);

  // Now simulate getting a message back from the other side.
  render_thread_->sink().ClearMessages();
  const int kPortId = 0;
  MiscellaneousBindings::DeliverMessage(
      extension_dispatcher_->v8_context_set().GetAll(),
      kPortId, "{\"val\": 42}", NULL);

  // Verify that we got it.
  render_thread_->VerifyRunJavaScriptMessageSend(
      ASCIIToUTF16("content got: 42"));
}

// Tests that the bindings for handling a new channel connection and channel
// closing all works.
TEST_F(ChromeRenderViewTest, ExtensionMessagesOnConnect) {
  extension_dispatcher_->SetTestExtensionId(kTestingExtensionId);
  LoadHTML("<body></body>");
  ExecuteJavaScript(
    "chrome.extension.onConnect.addListener(function (port) {"
    "  port.test = 24;"
    "  port.onMessage.addListener(doOnMessage);"
    "  port.onDisconnect.addListener(doOnDisconnect);"
    "  port.postMessage({message: 'onconnect from ' + port.tab.url + "
    "                   ' name ' + port.name});"
    "});"
    "function doOnMessage(msg, port) {"
    "  alert('got: ' + msg.val);"
    "}"
    "function doOnDisconnect(port) {"
    "  alert('disconnected: ' + port.test);"
    "}");

  render_thread_->sink().ClearMessages();

  // Simulate a new connection being opened.
  const int kPortId = 0;
  const std::string kPortName = "testName";
  DispatchOnConnect(extension_dispatcher_->v8_context_set(),
                    kPortId, kPortName, "{\"url\":\"foo://bar\"}");

  // Verify that we handled the new connection by posting a message.
  const IPC::Message* post_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          ExtensionHostMsg_PostMessage::ID);
  ASSERT_TRUE(post_msg);
  ExtensionHostMsg_PostMessage::Param post_params;
  ExtensionHostMsg_PostMessage::Read(post_msg, &post_params);
  std::string expected_msg =
      "{\"message\":\"onconnect from foo://bar name " + kPortName + "\"}";
  EXPECT_EQ(expected_msg, post_params.b);

  // Now simulate getting a message back from the channel opener.
  render_thread_->sink().ClearMessages();
  MiscellaneousBindings::DeliverMessage(
      extension_dispatcher_->v8_context_set().GetAll(),
      kPortId, "{\"val\": 42}", NULL);

  // Verify that we got it.
  render_thread_->VerifyRunJavaScriptMessageSend(ASCIIToUTF16("got: 42"));

  // Now simulate the channel closing.
  render_thread_->sink().ClearMessages();
  DispatchOnDisconnect(extension_dispatcher_->v8_context_set(), kPortId);

  // Verify that we got it.
  render_thread_->VerifyRunJavaScriptMessageSend(
      ASCIIToUTF16("disconnected: 24"));
}
