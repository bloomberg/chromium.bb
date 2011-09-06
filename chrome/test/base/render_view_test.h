// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_RENDER_VIEW_TEST_H_
#define CHROME_TEST_BASE_RENDER_VIEW_TEST_H_
#pragma once

#include <string>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/renderer/autofill/autofill_agent.h"
#include "chrome/renderer/mock_keyboard.h"
#include "chrome/renderer/mock_render_thread.h"
#include "content/common/main_function_params.h"
#include "content/common/native_web_keyboard_event.h"
#include "content/common/sandbox_init_wrapper.h"
#include "content/renderer/render_view.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

namespace autofill {
class AutofillAgent;
class PasswordAutofillManager;
}

class ExtensionRendererContext;
class MockRenderProcess;
class RendererMainPlatformDelegate;

class RenderViewTest : public testing::Test {
 public:
  // A special WebKitPlatformSupportImpl class for getting rid off the
  // dependency to the sandbox, which is not available in RenderViewTest.
  class RendererWebKitPlatformSupportImplNoSandbox :
      public RendererWebKitPlatformSupportImpl {
   public:
    virtual WebKit::WebSandboxSupport* sandboxSupport() {
      return NULL;
    }
  };

  RenderViewTest();
  virtual ~RenderViewTest();

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

  // Returns the bounds (coordinates and size) of the element with id
  // |element_id|.  Returns an empty rect if such an element was not found.
  gfx::Rect GetElementBounds(const std::string& element_id);

  // Sends a left mouse click in the middle of the element with id |element_id|.
  // Returns true if the event was sent, false otherwise (typically because
  // the element was not found).
  bool SimulateElementClick(const std::string& element_id);

  // Clears anything associated with the browsing history.
  void ClearHistory();

  // testing::Test
  virtual void SetUp();

  virtual void TearDown();

  MessageLoop msg_loop_;
  chrome::ChromeContentRendererClient chrome_content_renderer_client_;
  ExtensionRendererContext* extension_dispatcher_;
  MockRenderThread render_thread_;
  scoped_ptr<MockRenderProcess> mock_process_;
  scoped_refptr<RenderView> view_;
  RendererWebKitPlatformSupportImplNoSandbox webkit_platform_support_;
  scoped_ptr<MockKeyboard> mock_keyboard_;

  // Used to setup the process so renderers can run.
  scoped_ptr<RendererMainPlatformDelegate> platform_;
  scoped_ptr<MainFunctionParams> params_;
  scoped_ptr<CommandLine> command_line_;
  scoped_ptr<SandboxInitWrapper> sandbox_init_wrapper_;

  autofill::PasswordAutofillManager* password_autofill_;
  autofill::AutofillAgent* autofill_agent_;
};

#endif  // CHROME_TEST_BASE_RENDER_VIEW_TEST_H_
