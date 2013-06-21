// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/message_window.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class MessageWindowDelegate : public win::MessageWindow::Delegate {
 public:
  MessageWindowDelegate();
  virtual ~MessageWindowDelegate();

  // MessageWindow::Delegate interface.
  virtual bool HandleMessage(HWND hwnd,
                             UINT message,
                             WPARAM wparam,
                             LPARAM lparam,
                             LRESULT* result) OVERRIDE;
};

MessageWindowDelegate::MessageWindowDelegate() {
}

MessageWindowDelegate::~MessageWindowDelegate() {
}

bool MessageWindowDelegate::HandleMessage(
    HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, LRESULT* result) {
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
  MessageWindowDelegate delegate;
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(&delegate, NULL));
}

// Verifies that the created window can receive messages.
TEST(MessageWindowTest, SendMessage) {
  MessageWindowDelegate delegate;
  win::MessageWindow window;
  EXPECT_TRUE(window.Create(&delegate, NULL));

  EXPECT_EQ(SendMessage(window.hwnd(), WM_USER, 100, 0), 100);
}

}  // namespace base
