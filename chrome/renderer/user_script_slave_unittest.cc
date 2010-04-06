// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/user_script_slave.h"

#include "base/pickle.h"
#include "base/scoped_ptr.h"
#include "base/shared_memory.h"
#include "base/string_piece.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/mock_render_thread.h"
#include "chrome/renderer/render_thread.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sync_message.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef std::vector<UserScript::File> FileList;
typedef std::vector<UserScript*> UserScripts;

// Valid extension id.
const char* const kExtensionId = "nanifionbniojdbjhimhbiajcfajnjde";
// JS script.
const char* const kJsScript =
    "var x = { 'id': '__MSG_@@extension_id__'};";
// CSS style.
const char* const kCssScript =
    "div {url(chrome-extension://__MSG_@@extension_id__/icon.png);}";
const char* const kCssScriptWithReplacedMessage =
    "div {url(chrome-extension://nanifionbniojdbjhimhbiajcfajnjde/icon.png);}";

class UserScriptSlaveTest : public testing::Test {
 public:
  UserScriptSlaveTest()
      : shared_memory_(NULL) {
  }

  virtual void SetUp() {
    message_sender_.reset(new MockRenderThread());
    user_script_slave_.reset(new UserScriptSlave(message_sender_.get()));
  }

  virtual void TearDown() {
    shared_memory_.release();
  }

  UserScripts GetUserScripts() {
    return user_script_slave_->scripts_;
  }

 protected:
  // Create user script with given extension id.
  void CreateUserScript(const std::string extension_id) {
    // Extension id can be empty.
    user_script_.set_extension_id(extension_id);

    // Add js script.
    FileList& js_scripts = user_script_.js_scripts();
    UserScript::File js_script;
    js_script.set_content(base::StringPiece(kJsScript, strlen(kJsScript)));
    js_scripts.push_back(js_script);

    // Add css script.
    FileList& css_scripts = user_script_.css_scripts();
    UserScript::File css_script;
    css_script.set_content(base::StringPiece(kCssScript, strlen(kCssScript)));
    css_scripts.push_back(css_script);
  }

  // Serializes the UserScript object and stores it in the shared memory.
  bool Serialize(const UserScript& script) {
    Pickle pickle;
    pickle.WriteSize(1);
    script.Pickle(&pickle);
    for (size_t j = 0; j < script.js_scripts().size(); j++) {
      base::StringPiece contents = script.js_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }
    for (size_t j = 0; j < script.css_scripts().size(); j++) {
      base::StringPiece contents = script.css_scripts()[j].GetContent();
      pickle.WriteData(contents.data(), contents.length());
    }

    // Create the shared memory object.
    shared_memory_.reset(new base::SharedMemory());

    if (!shared_memory_->Create(std::wstring(),  // anonymous
                               false,  // read-only
                               false,  // open existing
                               pickle.size()))
      return false;

    // Map into our process.
    if (!shared_memory_->Map(pickle.size()))
      return false;

    // Copy the pickle to shared memory.
    memcpy(shared_memory_->memory(), pickle.data(), pickle.size());
    return true;
  }

  // User script slave we are testing.
  scoped_ptr<UserScriptSlave> user_script_slave_;

  // IPC message sender.
  scoped_ptr<MockRenderThread> message_sender_;

  // User script that has css and js files.
  UserScript user_script_;

  // Shared memory object used to pass user scripts from browser to renderer.
  scoped_ptr<base::SharedMemory> shared_memory_;
};

TEST_F(UserScriptSlaveTest, MessagesNotReplacedIfScriptDoesntHaveCssScript) {
  CreateUserScript(kExtensionId);
  user_script_.css_scripts().clear();

  ASSERT_TRUE(Serialize(user_script_));
  ASSERT_TRUE(user_script_slave_->UpdateScripts(
      shared_memory_->handle(), false));

  UserScripts scripts = GetUserScripts();
  ASSERT_EQ(1U, scripts.size());
  ASSERT_EQ(1U, scripts[0]->js_scripts().size());
  EXPECT_EQ(0U, scripts[0]->css_scripts().size());

  EXPECT_EQ(scripts[0]->js_scripts()[0].GetContent(), kJsScript);

  // Send was not called to fetch the messages in this case, since we don't
  // have the css scripts.
  EXPECT_EQ(0U, message_sender_->sink().message_count());
}

TEST_F(UserScriptSlaveTest, MessagesNotReplacedIfScriptDoesntHaveExtensionId) {
  CreateUserScript("");
  ASSERT_TRUE(Serialize(user_script_));
  ASSERT_TRUE(user_script_slave_->UpdateScripts(
      shared_memory_->handle(), false));

  UserScripts scripts = GetUserScripts();
  ASSERT_EQ(1U, scripts.size());
  ASSERT_EQ(1U, scripts[0]->js_scripts().size());
  ASSERT_EQ(1U, scripts[0]->css_scripts().size());

  EXPECT_EQ(scripts[0]->js_scripts()[0].GetContent(), kJsScript);
  EXPECT_EQ(scripts[0]->css_scripts()[0].GetContent(), kCssScript);

  // Send was not called to fetch the messages in this case, since we don't
  // have the extension id.
  EXPECT_EQ(0U, message_sender_->sink().message_count());
}

TEST_F(UserScriptSlaveTest, MessagesAreReplacedIfScriptHasExtensionId) {
  CreateUserScript(kExtensionId);
  ASSERT_TRUE(Serialize(user_script_));
  ASSERT_TRUE(user_script_slave_->UpdateScripts(
      shared_memory_->handle(), false));

  UserScripts scripts = GetUserScripts();
  ASSERT_EQ(1U, scripts.size());
  ASSERT_EQ(1U, scripts[0]->js_scripts().size());
  ASSERT_EQ(1U, scripts[0]->css_scripts().size());

  // JS scripts should stay the same.
  EXPECT_STREQ(scripts[0]->js_scripts()[0].GetContent().data(), kJsScript);
  // We expect only CSS scripts to change.
  EXPECT_STREQ(scripts[0]->css_scripts()[0].GetContent().data(),
               kCssScriptWithReplacedMessage);

  // Send gets always gets called if there is an extension id and css script.
  ASSERT_EQ(1U, message_sender_->sink().message_count());
  EXPECT_TRUE(message_sender_->sink().GetFirstMessageMatching(
      ViewHostMsg_GetExtensionMessageBundle::ID));
}
