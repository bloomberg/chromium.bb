// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/chrome/recorder_devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(JavaScriptDialogManager, NoDialog) {
  StubDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  std::string message("HI");
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_STREQ("HI", message.c_str());
  ASSERT_EQ(kNoAlertOpen, manager.HandleDialog(false, NULL).code());
}

TEST(JavaScriptDialogManager, HandleDialogPassesParams) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "hi");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  std::string given_text("text");
  ASSERT_EQ(kOk, manager.HandleDialog(false, &given_text).code());
  std::string text;
  ASSERT_TRUE(client.commands_[0].params.GetString("promptText", &text));
  ASSERT_EQ(given_text, text);
  ASSERT_TRUE(client.commands_[0].params.HasKey("accept"));
}

TEST(JavaScriptDialogManager, HandleDialogNullPrompt) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "hi");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_EQ(kOk, manager.HandleDialog(false, NULL).code());
  ASSERT_FALSE(client.commands_[0].params.HasKey("promptText"));
  ASSERT_TRUE(client.commands_[0].params.HasKey("accept"));
}

TEST(JavaScriptDialogManager, ReconnectClearsStateAndSendsEnable) {
  RecorderDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "hi");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());

  ASSERT_TRUE(manager.OnConnected(&client).IsOk());
  ASSERT_EQ("Page.enable", client.commands_[0].method);
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoAlertOpen, manager.HandleDialog(false, NULL).code());
}

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  FakeDevToolsClient() : listener_(NULL), closing_count_(0) {}
  ~FakeDevToolsClient() override {}

  void set_closing_count(int closing_count) {
    closing_count_ = closing_count;
  }

  // Overridden from StubDevToolsClient:
  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      scoped_ptr<base::DictionaryValue>* result) override {
    while (closing_count_ > 0) {
      base::DictionaryValue empty;
      Status status =
          listener_->OnEvent(this, "Page.javascriptDialogClosed", empty);
      if (status.IsError())
        return status;
      closing_count_--;
    }
    return Status(kOk);
  }
  void AddListener(DevToolsEventListener* listener) override {
    listener_ = listener;
  }

 private:
  DevToolsEventListener* listener_;
  int closing_count_;
};

}  // namespace

TEST(JavaScriptDialogManager, OneDialog) {
  FakeDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "hi");
  ASSERT_FALSE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ("hi", message);

  client.set_closing_count(1);
  ASSERT_EQ(kOk, manager.HandleDialog(false, NULL).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoAlertOpen, manager.HandleDialog(false, NULL).code());
}

TEST(JavaScriptDialogManager, TwoDialogs) {
  FakeDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "1");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  params.SetString("message", "2");
  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());

  std::string message;
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ("1", message);

  ASSERT_EQ(kOk, manager.HandleDialog(false, NULL).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ("2", message);

  client.set_closing_count(2);
  ASSERT_EQ(kOk, manager.HandleDialog(false, NULL).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoAlertOpen, manager.HandleDialog(false, NULL).code());
}

TEST(JavaScriptDialogManager, OneDialogManualClose) {
  StubDevToolsClient client;
  JavaScriptDialogManager manager(&client);
  base::DictionaryValue params;
  params.SetString("message", "hi");
  ASSERT_FALSE(manager.IsDialogOpen());
  std::string message;
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogOpening", params).code());
  ASSERT_TRUE(manager.IsDialogOpen());
  ASSERT_EQ(kOk, manager.GetDialogMessage(&message).code());
  ASSERT_EQ("hi", message);

  ASSERT_EQ(
      kOk,
      manager.OnEvent(&client, "Page.javascriptDialogClosed", params).code());
  ASSERT_FALSE(manager.IsDialogOpen());
  ASSERT_EQ(kNoAlertOpen, manager.GetDialogMessage(&message).code());
  ASSERT_EQ(kNoAlertOpen, manager.HandleDialog(false, NULL).code());
}
