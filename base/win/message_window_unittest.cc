// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/message_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

bool HandleMessage(
    UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
  // Return |wparam| as the result of WM_USER message.
  if (message == WM_USER) {
    *result = wparam;
    return true;
  }

  return false;
}

}  // namespace

// Checks that a window can be created.
TEST(MessageWindowTest, Create) {
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(base::Bind(&HandleMessage)));
}

TEST(MessageWindowTest, CreateNamed) {
  win::MessageWindow window;
  EXPECT_TRUE(window.CreateNamed(base::Bind(&HandleMessage),
              UTF8ToUTF16("test_message_window")));
}

// Verifies that the created window can receive messages.
TEST(MessageWindowTest, SendMessage) {
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(base::Bind(&HandleMessage)));

  EXPECT_EQ(SendMessage(window.hwnd(), WM_USER, 100, 0), 100);
}

}  // namespace base
