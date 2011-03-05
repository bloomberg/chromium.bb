// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_RENDER_VIEW_TEST_H_
#define CHROME_TEST_RENDER_VIEW_TEST_H_
#pragma once

#include <string>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "chrome/common/main_function_params.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/sandbox_init_wrapper.h"
#include "chrome/renderer/mock_keyboard.h"
#include "chrome/renderer/mock_render_thread.h"
#include "chrome/renderer/render_view.h"
#include "chrome/renderer/renderer_main_platform_delegate.h"
#include "chrome/renderer/renderer_webkitclient_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace autofill {
class AutoFillAgent;
class PasswordAutofillManager;
}
class MockRenderProcess;

class RenderViewTest : public testing::Test {
 public:
  // A special WebKitClientImpl class for getting rid off the dependency to the
  // sandbox, which is not available in RenderViewTest.
  class RendererWebKitClientImplNoSandbox : public RendererWebKitClientImpl {
   public:
    virtual WebKit::WebSandboxSupport* sandboxSupport() {
      return NULL;
    }
  };

  RenderViewTest();
  ~RenderViewTest();

 protected:
  // Spins the message loop to process all messages that are currently pending.
  void ProcessPendingMessages();

  // Returns a pointer to the main frame.
  WebKit::WebFrame* GetMainFrame();

  // Executes the given JavaScript in the context of the main frame. The input
  // is a NULL-terminated UTF-8 string.
  void ExecuteJavaScript(const char* js);

  // Executes the given JavaScript and sets the int value it evaluates to in
  // |result|.
  // Returns true if the JavaScript was evaluated correctly to an int value,
  // false otherwise.
  bool ExecuteJavaScriptAndReturnIntValue(const string16& script, int* result);

  // Loads the given HTML into the main frame as a data: URL.
  void LoadHTML(const char* html);

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers key_modifiers,
                   std::wstring* output);

  // Sends one native key event over IPC.
  void SendNativeKeyEvent(const NativeWebKeyboardEvent& key_event);

  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  void VerifyPageCount(int count);
  // Verifies the rendered "printed page".
  void VerifyPagesPrinted();

  // Returns the bounds (coordinates and size) of the element with id
  // |element_id|.  Returns an empty rect if such an element was not found.
  gfx::Rect GetElementBounds(const std::string& element_id);

  // Sends a left mouse click in the middle of the element with id |element_id|.
  // Returns true if the event was sent, false otherwise (typically because
  // the element was not found).
  bool SimulateElementClick(const std::string& element_id);

  // testing::Test
  virtual void SetUp();

  virtual void TearDown();

  MessageLoop msg_loop_;
  MockRenderThread render_thread_;
  scoped_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<RenderView> view_;
  RendererWebKitClientImplNoSandbox webkitclient_;
  scoped_ptr<MockKeyboard> mock_keyboard_;

  // Used to setup the process so renderers can run.
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;

  autofill::PasswordAutofillManager* password_autofill_;
  autofill::AutoFillAgent* autofill_agent_;
};

#endif  // CHROME_TEST_RENDER_VIEW_TEST_H_
