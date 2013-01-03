// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"

#include "base/shared_memory.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/common/intents_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/render_view_impl.h"
#include "content/shell/shell_content_browser_client.h"
#include "content/shell/shell_content_client.h"
#include "content/test/mock_keyboard.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIntentServiceInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/range/range.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/web_io_operators.h"

#if defined(OS_LINUX) && !defined(USE_AURA)
#include "ui/base/gtk/event_synthesis_gtk.h"
#endif

#if defined(USE_AURA)
#include "ui/base/events/event.h"
#endif

#if defined(USE_AURA) && defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/events/event_constants.h"
#include "ui/base/keycodes/keyboard_code_conversion.h"
#include "ui/base/x/x11_util.h"
#endif

using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebString;
using WebKit::WebTextDirection;
using WebKit::WebURLError;

namespace content  {

namespace {
#if defined(USE_AURA) && defined(USE_X11)
// Converts MockKeyboard::Modifiers to ui::EventFlags.
int ConvertMockKeyboardModifier(MockKeyboard::Modifiers modifiers) {
  static struct ModifierMap {
    MockKeyboard::Modifiers src;
    int dst;
  } kModifierMap[] = {
    { MockKeyboard::LEFT_SHIFT, ui::EF_SHIFT_DOWN },
    { MockKeyboard::RIGHT_SHIFT, ui::EF_SHIFT_DOWN },
    { MockKeyboard::LEFT_CONTROL, ui::EF_CONTROL_DOWN },
    { MockKeyboard::RIGHT_CONTROL, ui::EF_CONTROL_DOWN },
    { MockKeyboard::LEFT_ALT,  ui::EF_ALT_DOWN },
    { MockKeyboard::RIGHT_ALT, ui::EF_ALT_DOWN },
  };
  int flags = 0;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kModifierMap); ++i) {
    if (kModifierMap[i].src & modifiers) {
      flags |= kModifierMap[i].dst;
    }
  }
  return flags;
}
#endif

class WebUITestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  virtual WebUIController* CreateWebUIControllerForURL(
      WebUI* web_ui, const GURL& url) const OVERRIDE {
    return NULL;
  }
  virtual WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                                     const GURL& url) const OVERRIDE {
    return WebUI::kNoWebUI;
  }
  virtual bool UseWebUIForURL(BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE {
    return GetContentClient()->HasWebUIScheme(url);
  }
  virtual bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE {
    return GetContentClient()->HasWebUIScheme(url);
  }
  virtual bool IsURLAcceptableForWebUI(
      BrowserContext* browser_context,
      const GURL& url,
      bool data_urls_allowed) const OVERRIDE {
    return false;
  }
};

class WebUITestClient : public ShellContentClient {
 public:
  WebUITestClient() {
  }

  virtual bool HasWebUIScheme(const GURL& url) const OVERRIDE {
    return url.SchemeIs(chrome::kChromeUIScheme);
  }
};

class WebUITestBrowserClient : public ShellContentBrowserClient {
 public:
  WebUITestBrowserClient() {}

  virtual WebUIControllerFactory* GetWebUIControllerFactory() OVERRIDE {
    return &factory_;
  }

 private:
  WebUITestWebUIControllerFactory factory_;
};

}

class RenderViewImplTest : public RenderViewTest {
 public:
  RenderViewImplTest() {
    // Attach a pseudo keyboard device to this object.
    mock_keyboard_.reset(new MockKeyboard());
  }

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  // Sends IPC messages that emulates a key-press event.
  int SendKeyEvent(MockKeyboard::Layout layout,
                   int key_code,
                   MockKeyboard::Modifiers modifiers,
                   string16* output) {
#if defined(OS_WIN)
    // Retrieve the Unicode character for the given tuple (keyboard-layout,
    // key-code, and modifiers).
    // Exit when a keyboard-layout driver cannot assign a Unicode character to
    // the tuple to prevent sending an invalid key code to the RenderView object.
    CHECK(mock_keyboard_.get());
    CHECK(output);
    int length = mock_keyboard_->GetCharacters(layout, key_code, modifiers,
                                               output);
    if (length != 1)
      return -1;

    // Create IPC messages from Windows messages and send them to our
    // back-end.
    // A keyboard event of Windows consists of three Windows messages:
    // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
    // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
    // WM_CHAR sends a composed Unicode character.
    MSG msg1 = { NULL, WM_KEYDOWN, key_code, 0 };
#if defined(USE_AURA)
    ui::KeyEvent evt1(msg1, false);
    NativeWebKeyboardEvent keydown_event(&evt1);
#else
    NativeWebKeyboardEvent keydown_event(msg1);
#endif
    SendNativeKeyEvent(keydown_event);

    MSG msg2 = { NULL, WM_CHAR, (*output)[0], 0 };
#if defined(USE_AURA)
    ui::KeyEvent evt2(msg2, true);
    NativeWebKeyboardEvent char_event(&evt2);
#else
    NativeWebKeyboardEvent char_event(msg2);
#endif
    SendNativeKeyEvent(char_event);

    MSG msg3 = { NULL, WM_KEYUP, key_code, 0 };
#if defined(USE_AURA)
    ui::KeyEvent evt3(msg3, false);
    NativeWebKeyboardEvent keyup_event(&evt3);
#else
    NativeWebKeyboardEvent keyup_event(msg3);
#endif
    SendNativeKeyEvent(keyup_event);

    return length;
#elif defined(USE_AURA) && defined(USE_X11)
    // We ignore |layout|, which means we are only testing the layout of the
    // current locale. TODO(mazda): fix this to respect |layout|.
    CHECK(output);
    const int flags = ConvertMockKeyboardModifier(modifiers);

    XEvent xevent1;
    InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                            static_cast<ui::KeyboardCode>(key_code),
                            flags,
                            &xevent1);
    ui::KeyEvent event1(&xevent1, false);
    NativeWebKeyboardEvent keydown_event(&event1);
    SendNativeKeyEvent(keydown_event);

    XEvent xevent2;
    InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                            static_cast<ui::KeyboardCode>(key_code),
                            flags,
                            &xevent2);
    ui::KeyEvent event2(&xevent2, true);
    NativeWebKeyboardEvent char_event(&event2);
    SendNativeKeyEvent(char_event);

    XEvent xevent3;
    InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                            static_cast<ui::KeyboardCode>(key_code),
                            flags,
                            &xevent3);
    ui::KeyEvent event3(&xevent3, false);
    NativeWebKeyboardEvent keyup_event(&event3);
    SendNativeKeyEvent(keyup_event);

    long c = GetCharacterFromKeyCode(static_cast<ui::KeyboardCode>(key_code),
                                     flags);
    output->assign(1, static_cast<char16>(c));
    return 1;
#elif defined(OS_LINUX)
    // We ignore |layout|, which means we are only testing the layout of the
    // current locale. TODO(estade): fix this to respect |layout|.
    std::vector<GdkEvent*> events;
    ui::SynthesizeKeyPressEvents(
        NULL, static_cast<ui::KeyboardCode>(key_code),
        modifiers & (MockKeyboard::LEFT_CONTROL | MockKeyboard::RIGHT_CONTROL),
        modifiers & (MockKeyboard::LEFT_SHIFT | MockKeyboard::RIGHT_SHIFT),
        modifiers & (MockKeyboard::LEFT_ALT | MockKeyboard::RIGHT_ALT),
        &events);

    guint32 unicode_key = 0;
    for (size_t i = 0; i < events.size(); ++i) {
      // Only send the up/down events for key press itself (skip the up/down
      // events for the modifier keys).
      if ((i + 1) == (events.size() / 2) || i == (events.size() / 2)) {
        unicode_key = gdk_keyval_to_unicode(events[i]->key.keyval);
        NativeWebKeyboardEvent webkit_event(events[i]);
        SendNativeKeyEvent(webkit_event);

        // Need to add a char event after the key down.
        if (webkit_event.type == WebKit::WebInputEvent::RawKeyDown) {
          NativeWebKeyboardEvent char_event = webkit_event;
          char_event.type = WebKit::WebInputEvent::Char;
          char_event.skip_in_browser = true;
          SendNativeKeyEvent(char_event);
        }
      }
      gdk_event_free(events[i]);
    }

    output->assign(1, static_cast<char16>(unicode_key));
    return 1;
#else
    NOTIMPLEMENTED();
    return L'\0';
#endif
  }

 private:
  scoped_ptr<MockKeyboard> mock_keyboard_;
};

// Test that we get form state change notifications when input fields change.
TEST_F(RenderViewImplTest, DISABLED_OnNavStateChanged) {
  // Don't want any delay for form state sync changes. This will still post a
  // message so updates will get coalesced, but as soon as we spin the message
  // loop, it will generate an update.
  view()->set_send_content_state_immediately(true);

  LoadHTML("<input type=\"text\" id=\"elt_text\"></input>");

  // We should NOT have gotten a form state change notification yet.
  EXPECT_FALSE(render_thread_->sink().GetFirstMessageMatching(
      ViewHostMsg_UpdateState::ID));
  render_thread_->sink().ClearMessages();

  // Change the value of the input. We should have gotten an update state
  // notification. We need to spin the message loop to catch this update.
  ExecuteJavaScript("document.getElementById('elt_text').value = 'foo';");
  ProcessPendingMessages();
  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID));
}

TEST_F(RenderViewImplTest, OnNavigationHttpPost) {
  ViewMsg_Navigate_Params nav_params;

  // An http url will trigger a resource load so cannot be used here.
  nav_params.url = GURL("data:text/html,<div>Page</div>");
  nav_params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  nav_params.transition = PAGE_TRANSITION_TYPED;
  nav_params.page_id = -1;
  nav_params.is_post = true;

  // Set up post data.
  const unsigned char* raw_data = reinterpret_cast<const unsigned char*>(
      "post \0\ndata");
  const unsigned int length = 11;
  const std::vector<unsigned char> post_data(raw_data, raw_data + length);
  nav_params.browser_initiated_post_data = post_data;

  view()->OnNavigate(nav_params);
  ProcessPendingMessages();

  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          ViewHostMsg_FrameNavigate::ID);
  EXPECT_TRUE(frame_navigate_msg);

  ViewHostMsg_FrameNavigate::Param host_nav_params;
  ViewHostMsg_FrameNavigate::Read(frame_navigate_msg, &host_nav_params);
  EXPECT_TRUE(host_nav_params.a.is_post);

  // Check post data sent to browser matches
  EXPECT_FALSE(host_nav_params.a.content_state.empty());
  const WebKit::WebHistoryItem item = webkit_glue::HistoryItemFromString(
      host_nav_params.a.content_state);
  WebKit::WebHTTPBody body = item.httpBody();
  WebKit::WebHTTPBody::Element element;
  bool successful = body.elementAt(0, element);
  EXPECT_TRUE(successful);
  EXPECT_EQ(WebKit::WebHTTPBody::Element::TypeData, element.type);
  EXPECT_EQ(length, element.data.size());
  EXPECT_EQ(0, memcmp(raw_data, element.data.data(), length));
}

TEST_F(RenderViewImplTest, DecideNavigationPolicy) {
  WebUITestClient client;
  WebUITestBrowserClient browser_client;
  ContentClient* old_client = GetContentClient();
  ContentBrowserClient* old_browser_client = GetContentClient()->browser();

  SetContentClient(&client);
  GetContentClient()->set_browser_for_testing(&browser_client);
  client.set_renderer_for_testing(old_client->renderer());

  // Navigations to normal HTTP URLs can be handled locally.
  WebKit::WebURLRequest request(GURL("http://foo.com"));
  WebKit::WebNavigationPolicy policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      request,
      WebKit::WebNavigationTypeLinkClicked,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyCurrentTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyCurrentTab, policy);

  // Verify that form posts to WebUI URLs will be sent to the browser process.
  WebKit::WebURLRequest form_request(GURL("chrome://foo"));
  form_request.setHTTPMethod("POST");
  policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      form_request,
      WebKit::WebNavigationTypeFormSubmitted,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyCurrentTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  // Verify that popup links to WebUI URLs also are sent to browser.
  WebKit::WebURLRequest popup_request(GURL("chrome://foo"));
  policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      popup_request,
      WebKit::WebNavigationTypeLinkClicked,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyNewForegroundTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  GetContentClient()->set_browser_for_testing(old_browser_client);
  SetContentClient(old_client);
}

TEST_F(RenderViewImplTest, DecideNavigationPolicyForWebUI) {
  // Enable bindings to simulate a WebUI view.
  view()->OnAllowBindings(BINDINGS_POLICY_WEB_UI);

  // Navigations to normal HTTP URLs will be sent to browser process.
  WebKit::WebURLRequest request(GURL("http://foo.com"));
  WebKit::WebNavigationPolicy policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      request,
      WebKit::WebNavigationTypeLinkClicked,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyCurrentTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  // Navigations to WebUI URLs will also be sent to browser process.
  WebKit::WebURLRequest webui_request(GURL("chrome://foo"));
  policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      webui_request,
      WebKit::WebNavigationTypeLinkClicked,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyCurrentTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  // Verify that form posts to data URLs will be sent to the browser process.
  WebKit::WebURLRequest data_request(GURL("data:text/html,foo"));
  data_request.setHTTPMethod("POST");
  policy = view()->decidePolicyForNavigation(
      GetMainFrame(),
      data_request,
      WebKit::WebNavigationTypeFormSubmitted,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyCurrentTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  // Verify that a popup that creates a view first and then navigates to a
  // normal HTTP URL will be sent to the browser process, even though the
  // new view does not have any enabled_bindings_.
  WebKit::WebURLRequest popup_request(GURL("http://foo.com"));
  WebKit::WebView* new_web_view = view()->createView(
      GetMainFrame(), popup_request, WebKit::WebWindowFeatures(), "foo",
      WebKit::WebNavigationPolicyNewForegroundTab);
  RenderViewImpl* new_view = RenderViewImpl::FromWebView(new_web_view);
  policy = new_view->decidePolicyForNavigation(
      new_web_view->mainFrame(),
      popup_request,
      WebKit::WebNavigationTypeLinkClicked,
      WebKit::WebNode(),
      WebKit::WebNavigationPolicyNewForegroundTab,
      false);
  EXPECT_EQ(WebKit::WebNavigationPolicyIgnore, policy);

  // Clean up after the new view so we don't leak it.
  new_view->Close();
  new_view->Release();
}

// Ensure the RenderViewImpl sends an ACK to a SwapOut request, even if it is
// already swapped out.  http://crbug.com/93427.
TEST_F(RenderViewImplTest, SendSwapOutACK) {
  LoadHTML("<div>Page A</div>");
  int initial_page_id = view()->GetPageId();

  // Respond to a swap out request.
  ViewMsg_SwapOut_Params params;
  params.closing_process_id = 10;
  params.closing_route_id = 11;
  params.new_render_process_host_id = 12;
  params.new_request_id = 13;
  view()->OnSwapOut(params);

  // Ensure the swap out commits synchronously.
  EXPECT_NE(initial_page_id, view()->GetPageId());

  // Check for a valid OnSwapOutACK with echoed params.
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_SwapOut_ACK::ID);
  ASSERT_TRUE(msg);
  ViewHostMsg_SwapOut_ACK::Param reply_params;
  ViewHostMsg_SwapOut_ACK::Read(msg, &reply_params);
  EXPECT_EQ(params.closing_process_id, reply_params.a.closing_process_id);
  EXPECT_EQ(params.closing_route_id, reply_params.a.closing_route_id);
  EXPECT_EQ(params.new_render_process_host_id,
            reply_params.a.new_render_process_host_id);
  EXPECT_EQ(params.new_request_id, reply_params.a.new_request_id);

  // It is possible to get another swap out request.  Ensure that we send
  // an ACK, even if we don't have to do anything else.
  render_thread_->sink().ClearMessages();
  view()->OnSwapOut(params);
  const IPC::Message* msg2 = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_SwapOut_ACK::ID);
  ASSERT_TRUE(msg2);

  // If we navigate back to this RenderView, ensure we don't send a state
  // update for the swapped out URL.  (http://crbug.com/72235)
  ViewMsg_Navigate_Params nav_params;
  nav_params.url = GURL("data:text/html,<div>Page B</div>");
  nav_params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  nav_params.transition = PAGE_TRANSITION_TYPED;
  nav_params.current_history_list_length = 1;
  nav_params.current_history_list_offset = 0;
  nav_params.pending_history_list_offset = 1;
  nav_params.page_id = -1;
  view()->OnNavigate(nav_params);
  ProcessPendingMessages();
  const IPC::Message* msg3 = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  EXPECT_FALSE(msg3);
}

// Ensure the RenderViewImpl reloads the previous page if a reload request
// arrives while it is showing swappedout://.  http://crbug.com/143155.
TEST_F(RenderViewImplTest, ReloadWhileSwappedOut) {
  // Load page A.
  LoadHTML("<div>Page A</div>");

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");

  // Check for a valid UpdateState message for page A.
  ProcessPendingMessages();
  const IPC::Message* msg_A = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  int page_id_A;
  std::string state_A;
  ViewHostMsg_UpdateState::Read(msg_A, &page_id_A, &state_A);
  EXPECT_EQ(1, page_id_A);
  render_thread_->sink().ClearMessages();

  // Back to page A (page_id 1) and commit.
  ViewMsg_Navigate_Params params_A;
  params_A.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_A.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_A.current_history_list_length = 2;
  params_A.current_history_list_offset = 1;
  params_A.pending_history_list_offset = 0;
  params_A.page_id = 1;
  params_A.state = state_A;
  view()->OnNavigate(params_A);
  ProcessPendingMessages();

  // Respond to a swap out request.
  ViewMsg_SwapOut_Params params;
  params.closing_process_id = 10;
  params.closing_route_id = 11;
  params.new_render_process_host_id = 12;
  params.new_request_id = 13;
  view()->OnSwapOut(params);

  // Check for a OnSwapOutACK.
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_SwapOut_ACK::ID);
  ASSERT_TRUE(msg);
  render_thread_->sink().ClearMessages();

  // It is possible to get a reload request at this point, containing the
  // params.state of the initial page (e.g., if the new page fails the
  // provisional load in the renderer process, after we unload the old page).
  // Ensure the old page gets reloaded, not swappedout://.
  ViewMsg_Navigate_Params nav_params;
  nav_params.url = GURL("data:text/html,<div>Page A</div>");
  nav_params.navigation_type = ViewMsg_Navigate_Type::RELOAD;
  nav_params.transition = PAGE_TRANSITION_RELOAD;
  nav_params.current_history_list_length = 2;
  nav_params.current_history_list_offset = 0;
  nav_params.pending_history_list_offset = 0;
  nav_params.page_id = 1;
  nav_params.state = state_A;
  view()->OnNavigate(nav_params);
  ProcessPendingMessages();

  // Verify page A committed, not swappedout://.
  const IPC::Message* frame_navigate_msg =
      render_thread_->sink().GetUniqueMessageMatching(
          ViewHostMsg_FrameNavigate::ID);
  EXPECT_TRUE(frame_navigate_msg);

  // Read URL out of the parent trait of the params object.
  ViewHostMsg_FrameNavigate::Param commit_params;
  ViewHostMsg_FrameNavigate::Read(frame_navigate_msg, &commit_params);
  EXPECT_NE(GURL("swappedout://"), commit_params.a.url);
}


// Test that we get the correct UpdateState message when we go back twice
// quickly without committing.  Regression test for http://crbug.com/58082.
// Disabled: http://crbug.com/157357 .
TEST_F(RenderViewImplTest,  DISABLED_LastCommittedUpdateState) {
  // Load page A.
  LoadHTML("<div>Page A</div>");

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");

  // Check for a valid UpdateState message for page A.
  ProcessPendingMessages();
  const IPC::Message* msg_A = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  int page_id_A;
  std::string state_A;
  ViewHostMsg_UpdateState::Read(msg_A, &page_id_A, &state_A);
  EXPECT_EQ(1, page_id_A);
  render_thread_->sink().ClearMessages();

  // Load page C, which will trigger an UpdateState message for page B.
  LoadHTML("<div>Page C</div>");

  // Check for a valid UpdateState for page B.
  ProcessPendingMessages();
  const IPC::Message* msg_B = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_B);
  int page_id_B;
  std::string state_B;
  ViewHostMsg_UpdateState::Read(msg_B, &page_id_B, &state_B);
  EXPECT_EQ(2, page_id_B);
  EXPECT_NE(state_A, state_B);
  render_thread_->sink().ClearMessages();

  // Load page D, which will trigger an UpdateState message for page C.
  LoadHTML("<div>Page D</div>");

  // Check for a valid UpdateState for page C.
  ProcessPendingMessages();
  const IPC::Message* msg_C = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_C);
  int page_id_C;
  std::string state_C;
  ViewHostMsg_UpdateState::Read(msg_C, &page_id_C, &state_C);
  EXPECT_EQ(3, page_id_C);
  EXPECT_NE(state_B, state_C);
  render_thread_->sink().ClearMessages();

  // Go back to C and commit, preparing for our real test.
  ViewMsg_Navigate_Params params_C;
  params_C.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_C.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_C.current_history_list_length = 4;
  params_C.current_history_list_offset = 3;
  params_C.pending_history_list_offset = 2;
  params_C.page_id = 3;
  params_C.state = state_C;
  view()->OnNavigate(params_C);
  ProcessPendingMessages();
  render_thread_->sink().ClearMessages();

  // Go back twice quickly, such that page B does not have a chance to commit.
  // This leads to two changes to the back/forward list but only one change to
  // the RenderView's page ID.

  // Back to page B (page_id 2), without committing.
  ViewMsg_Navigate_Params params_B;
  params_B.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_B.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_B.current_history_list_length = 4;
  params_B.current_history_list_offset = 2;
  params_B.pending_history_list_offset = 1;
  params_B.page_id = 2;
  params_B.state = state_B;
  view()->OnNavigate(params_B);

  // Back to page A (page_id 1) and commit.
  ViewMsg_Navigate_Params params;
  params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_B.current_history_list_length = 4;
  params_B.current_history_list_offset = 2;
  params_B.pending_history_list_offset = 0;
  params.page_id = 1;
  params.state = state_A;
  view()->OnNavigate(params);
  ProcessPendingMessages();

  // Now ensure that the UpdateState message we receive is consistent
  // and represents page C in both page_id and state.
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg);
  int page_id;
  std::string state;
  ViewHostMsg_UpdateState::Read(msg, &page_id, &state);
  EXPECT_EQ(page_id_C, page_id);
  EXPECT_NE(state_A, state);
  EXPECT_NE(state_B, state);
  EXPECT_EQ(state_C, state);
}

// Test that the history_page_ids_ list can reveal when a stale back/forward
// navigation arrives from the browser and can be ignored.  See
// http://crbug.com/86758.
TEST_F(RenderViewImplTest, StaleNavigationsIgnored) {
  // Load page A.
  LoadHTML("<div>Page A</div>");
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(1, view()->history_page_ids_[0]);

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(2, view()->history_page_ids_[1]);

  // Check for a valid UpdateState message for page A.
  ProcessPendingMessages();
  const IPC::Message* msg_A = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  int page_id_A;
  std::string state_A;
  ViewHostMsg_UpdateState::Read(msg_A, &page_id_A, &state_A);
  EXPECT_EQ(1, page_id_A);
  render_thread_->sink().ClearMessages();

  // Back to page A (page_id 1) and commit.
  ViewMsg_Navigate_Params params_A;
  params_A.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_A.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_A.current_history_list_length = 2;
  params_A.current_history_list_offset = 1;
  params_A.pending_history_list_offset = 0;
  params_A.page_id = 1;
  params_A.state = state_A;
  view()->OnNavigate(params_A);
  ProcessPendingMessages();

  // A new navigation commits, clearing the forward history.
  LoadHTML("<div>Page C</div>");
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(3, view()->history_page_ids_[1]);

  // The browser then sends a stale navigation to B, which should be ignored.
  ViewMsg_Navigate_Params params_B;
  params_B.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_B.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_B.current_history_list_length = 2;
  params_B.current_history_list_offset = 0;
  params_B.pending_history_list_offset = 1;
  params_B.page_id = 2;
  params_B.state = state_A;  // Doesn't matter, just has to be present.
  view()->OnNavigate(params_B);

  // State should be unchanged.
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(3, view()->history_page_ids_[1]);
}

// Test that we do not ignore navigations after the entry limit is reached,
// in which case the browser starts dropping entries from the front.  In this
// case, we'll see a page_id mismatch but the RenderView's id will be older,
// not newer, than params.page_id.  Use this as a cue that we should update the
// state and not treat it like a navigation to a cropped forward history item.
// See http://crbug.com/89798.
TEST_F(RenderViewImplTest, DontIgnoreBackAfterNavEntryLimit) {
  // Load page A.
  LoadHTML("<div>Page A</div>");
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(1, view()->history_page_ids_[0]);

  // Load page B, which will trigger an UpdateState message for page A.
  LoadHTML("<div>Page B</div>");
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(2, view()->history_page_ids_[1]);

  // Check for a valid UpdateState message for page A.
  ProcessPendingMessages();
  const IPC::Message* msg_A = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_A);
  int page_id_A;
  std::string state_A;
  ViewHostMsg_UpdateState::Read(msg_A, &page_id_A, &state_A);
  EXPECT_EQ(1, page_id_A);
  render_thread_->sink().ClearMessages();

  // Load page C, which will trigger an UpdateState message for page B.
  LoadHTML("<div>Page C</div>");
  EXPECT_EQ(3, view()->history_list_length_);
  EXPECT_EQ(2, view()->history_list_offset_);
  EXPECT_EQ(3, view()->history_page_ids_[2]);

  // Check for a valid UpdateState message for page B.
  ProcessPendingMessages();
  const IPC::Message* msg_B = render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_UpdateState::ID);
  ASSERT_TRUE(msg_B);
  int page_id_B;
  std::string state_B;
  ViewHostMsg_UpdateState::Read(msg_B, &page_id_B, &state_B);
  EXPECT_EQ(2, page_id_B);
  render_thread_->sink().ClearMessages();

  // Suppose the browser has limited the number of NavigationEntries to 2.
  // It has now dropped the first entry, but the renderer isn't notified.
  // Ensure that going back to page B (page_id 2) at offset 0 is successful.
  ViewMsg_Navigate_Params params_B;
  params_B.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  params_B.transition = PAGE_TRANSITION_FORWARD_BACK;
  params_B.current_history_list_length = 2;
  params_B.current_history_list_offset = 1;
  params_B.pending_history_list_offset = 0;
  params_B.page_id = 2;
  params_B.state = state_B;
  view()->OnNavigate(params_B);
  ProcessPendingMessages();

  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(2, view()->history_page_ids_[0]);
}

// Test that our IME backend sends a notification message when the input focus
// changes.
TEST_F(RenderViewImplTest, OnImeStateChanged) {
  // Enable our IME backend code.
  view()->OnSetInputMethodActive(true);

  // Load an HTML page consisting of two input fields.
  view()->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" type=\"text\" value=\"some text\"></input>"
           "<input id=\"test2\" type=\"password\"></input>"
           "</body>"
           "</html>");
  render_thread_->sink().ClearMessages();

  const int kRepeatCount = 10;
  for (int i = 0; i < kRepeatCount; i++) {
    // Move the input focus to the first <input> element, where we should
    // activate IMEs.
    ExecuteJavaScript("document.getElementById('test1').focus();");
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to activate IMEs.
    view()->UpdateTextInputState(RenderWidget::DO_NOT_SHOW_IME);
    const IPC::Message* msg = render_thread_->sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_TextInputStateChanged::ID, msg->type());
    ViewHostMsg_TextInputStateChanged::Param params;
    ViewHostMsg_TextInputStateChanged::Read(msg, &params);
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, params.a.type);
    EXPECT_EQ(true, params.a.can_compose_inline);
    EXPECT_EQ("some text", params.a.value);
    EXPECT_EQ(0, params.a.selection_start);
    EXPECT_EQ(9, params.a.selection_end);
    EXPECT_EQ(-1, params.a.composition_start);
    EXPECT_EQ(-1, params.a.composition_end);

    // Move the input focus to the second <input> element, where we should
    // de-activate IMEs.
    ExecuteJavaScript("document.getElementById('test2').focus();");
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    // Update the IME status and verify if our IME backend sends an IPC message
    // to de-activate IMEs.
    view()->UpdateTextInputState(RenderWidget::DO_NOT_SHOW_IME);
    msg = render_thread_->sink().GetMessageAt(0);
    EXPECT_TRUE(msg != NULL);
    EXPECT_EQ(ViewHostMsg_TextInputStateChanged::ID, msg->type());
    ViewHostMsg_TextInputStateChanged::Read(msg, &params);
    EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, params.a.type);
  }
}

// Test that our IME backend can compose CJK words.
// Our IME front-end sends many platform-independent messages to the IME backend
// while it composes CJK words. This test sends the minimal messages captured
// on my local environment directly to the IME backend to verify if the backend
// can compose CJK words without any problems.
// This test uses an array of command sets because an IME composotion does not
// only depends on IME events, but also depends on window events, e.g. moving
// the window focus while composing a CJK text. To handle such complicated
// cases, this test should not only call IME-related functions in the
// RenderWidget class, but also call some RenderWidget members, e.g.
// ExecuteJavaScript(), RenderWidget::OnSetFocus(), etc.
TEST_F(RenderViewImplTest, ImeComposition) {
  enum ImeCommand {
    IME_INITIALIZE,
    IME_SETINPUTMODE,
    IME_SETFOCUS,
    IME_SETCOMPOSITION,
    IME_CONFIRMCOMPOSITION,
    IME_CANCELCOMPOSITION
  };
  struct ImeMessage {
    ImeCommand command;
    bool enable;
    int selection_start;
    int selection_end;
    const wchar_t* ime_string;
    const wchar_t* result;
  };
  static const ImeMessage kImeMessages[] = {
    // Scenario 1: input a Chinese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 1, 1, L"n", L"n"},
    {IME_SETCOMPOSITION, false, 2, 2, L"ni", L"ni"},
    {IME_SETCOMPOSITION, false, 3, 3, L"nih", L"nih"},
    {IME_SETCOMPOSITION, false, 4, 4, L"niha", L"niha"},
    {IME_SETCOMPOSITION, false, 5, 5, L"nihao", L"nihao"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"\x4F60\x597D", L"\x4F60\x597D"},
    // Scenario 2: input a Japanese word with Microsoft IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xFF4B", L"\xFF4B"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x304B", L"\x304B"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x304B\xFF4E", L"\x304B\xFF4E"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\xFF4A",
     L"\x304B\x3093\xFF4A"},
    {IME_SETCOMPOSITION, false, 0, 3, L"\x304B\x3093\x3058",
     L"\x304B\x3093\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x611F\x3058", L"\x611F\x3058"},
    {IME_SETCOMPOSITION, false, 0, 2, L"\x6F22\x5B57", L"\x6F22\x5B57"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\x6F22\x5B57"},
    // Scenario 3: input a Korean word with Microsot IME (on Vista).
    {IME_INITIALIZE, true, 0, 0, NULL, NULL},
    {IME_SETINPUTMODE, true, 0, 0, NULL, NULL},
    {IME_SETFOCUS, true, 0, 0, NULL, NULL},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3147", L"\x3147"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC544", L"\xC544"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xC548", L"\xC548"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\x3134", L"\xC548\x3134"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB140", L"\xC548\xB140"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CANCELCOMPOSITION, false, -1, -1, L"", L"\xC548"},
    {IME_SETCOMPOSITION, false, 0, 1, L"\xB155", L"\xC548\xB155"},
    {IME_CONFIRMCOMPOSITION, false, -1, -1, L"", L"\xC548\xB155"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kImeMessages); i++) {
    const ImeMessage* ime_message = &kImeMessages[i];
    switch (ime_message->command) {
      case IME_INITIALIZE:
        // Load an HTML page consisting of a content-editable <div> element,
        // and move the input focus to the <div> element, where we can use
        // IMEs.
        view()->OnSetInputMethodActive(ime_message->enable);
        view()->set_send_content_state_immediately(true);
        LoadHTML("<html>"
                "<head>"
                "</head>"
                "<body>"
                "<div id=\"test1\" contenteditable=\"true\"></div>"
                "</body>"
                "</html>");
        ExecuteJavaScript("document.getElementById('test1').focus();");
        break;

      case IME_SETINPUTMODE:
        // Activate (or deactivate) our IME back-end.
        view()->OnSetInputMethodActive(ime_message->enable);
        break;

      case IME_SETFOCUS:
        // Update the window focus.
        view()->OnSetFocus(ime_message->enable);
        break;

      case IME_SETCOMPOSITION:
        view()->OnImeSetComposition(
            WideToUTF16Hack(ime_message->ime_string),
            std::vector<WebKit::WebCompositionUnderline>(),
            ime_message->selection_start,
            ime_message->selection_end);
        break;

      case IME_CONFIRMCOMPOSITION:
        view()->OnImeConfirmComposition(
            WideToUTF16Hack(ime_message->ime_string),
            ui::Range::InvalidRange());
        break;

      case IME_CANCELCOMPOSITION:
        view()->OnImeSetComposition(
            string16(),
            std::vector<WebKit::WebCompositionUnderline>(),
            0, 0);
        break;
    }

    // Update the status of our IME back-end.
    // TODO(hbono): we should verify messages to be sent from the back-end.
    view()->UpdateTextInputState(RenderWidget::DO_NOT_SHOW_IME);
    ProcessPendingMessages();
    render_thread_->sink().ClearMessages();

    if (ime_message->result) {
      // Retrieve the content of this page and compare it with the expected
      // result.
      const int kMaxOutputCharacters = 128;
      std::wstring output = UTF16ToWideHack(
          GetMainFrame()->contentAsText(kMaxOutputCharacters));
      EXPECT_EQ(output, ime_message->result);
    }
  }
}

// Test that the RenderView::OnSetTextDirection() function can change the text
// direction of the selected input element.
TEST_F(RenderViewImplTest, OnSetTextDirection) {
  // Load an HTML page consisting of a <textarea> element and a <div> element.
  // This test changes the text direction of the <textarea> element, and
  // writes the values of its 'dir' attribute and its 'direction' property to
  // verify that the text direction is changed.
  view()->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<textarea id=\"test\"></textarea>"
           "<div id=\"result\" contenteditable=\"true\"></div>"
           "</body>"
           "</html>");
  render_thread_->sink().ClearMessages();

  static const struct {
    WebTextDirection direction;
    const wchar_t* expected_result;
  } kTextDirection[] = {
    { WebKit::WebTextDirectionRightToLeft, L"\x000A" L"rtl,rtl" },
    { WebKit::WebTextDirectionLeftToRight, L"\x000A" L"ltr,ltr" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTextDirection); ++i) {
    // Set the text direction of the <textarea> element.
    ExecuteJavaScript("document.getElementById('test').focus();");
    view()->OnSetTextDirection(kTextDirection[i].direction);

    // Write the values of its DOM 'dir' attribute and its CSS 'direction'
    // property to the <div> element.
    ExecuteJavaScript("var result = document.getElementById('result');"
                      "var node = document.getElementById('test');"
                      "var style = getComputedStyle(node, null);"
                      "result.innerText ="
                      "    node.getAttribute('dir') + ',' +"
                      "    style.getPropertyValue('direction');");

    // Copy the document content to std::wstring and compare with the
    // expected result.
    const int kMaxOutputCharacters = 16;
    std::wstring output = UTF16ToWideHack(
        GetMainFrame()->contentAsText(kMaxOutputCharacters));
    EXPECT_EQ(output, kTextDirection[i].expected_result);
  }
}

// Test that we can receive correct DOM events when we send input events
// through the RenderWidget::OnHandleInputEvent() function.
TEST_F(RenderViewImplTest, OnHandleKeyboardEvent) {
#if !defined(OS_MACOSX)
  // Load an HTML page consisting of one <input> element and three
  // contentediable <div> elements.
  // The <input> element is used for sending keyboard events, and the <div>
  // elements are used for writing DOM events in the following format:
  //   "<keyCode>,<shiftKey>,<controlKey>,<altKey>".
  // TODO(hbono): <http://crbug.com/2215> Our WebKit port set |ev.metaKey| to
  // true when pressing an alt key, i.e. the |ev.metaKey| value is not
  // trustworthy. We will check the |ev.metaKey| value when this issue is fixed.
  view()->set_send_content_state_immediately(true);
  LoadHTML("<html>"
           "<head>"
           "<title></title>"
           "<script type='text/javascript' language='javascript'>"
           "function OnKeyEvent(ev) {"
           "  var result = document.getElementById(ev.type);"
           "  result.innerText ="
           "      (ev.which || ev.keyCode) + ',' +"
           "      ev.shiftKey + ',' +"
           "      ev.ctrlKey + ',' +"
           "      ev.altKey;"
           "  return true;"
           "}"
           "</script>"
           "</head>"
           "<body>"
           "<input id='test' type='text'"
           "    onkeydown='return OnKeyEvent(event);'"
           "    onkeypress='return OnKeyEvent(event);'"
           "    onkeyup='return OnKeyEvent(event);'>"
           "</input>"
           "<div id='keydown' contenteditable='true'>"
           "</div>"
           "<div id='keypress' contenteditable='true'>"
           "</div>"
           "<div id='keyup' contenteditable='true'>"
           "</div>"
           "</body>"
           "</html>");
  ExecuteJavaScript("document.getElementById('test').focus();");
  render_thread_->sink().ClearMessages();

  static const MockKeyboard::Layout kLayouts[] = {
#if defined(OS_WIN)
    // Since we ignore the mock keyboard layout on Linux and instead just use
    // the screen's keyboard layout, these trivially pass. They are commented
    // out to avoid the illusion that they work.
    MockKeyboard::LAYOUT_ARABIC,
    MockKeyboard::LAYOUT_CANADIAN_FRENCH,
    MockKeyboard::LAYOUT_FRENCH,
    MockKeyboard::LAYOUT_HEBREW,
    MockKeyboard::LAYOUT_RUSSIAN,
#endif
    MockKeyboard::LAYOUT_UNITED_STATES,
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLayouts); ++i) {
    // For each key code, we send three keyboard events.
    //  * we press only the key;
    //  * we press the key and a left-shift key, and;
    //  * we press the key and a right-alt (AltGr) key.
    // For each modifiers, we need a string used for formatting its expected
    // result. (See the above comment for its format.)
    static const struct {
      MockKeyboard::Modifiers modifiers;
      const char* expected_result;
    } kModifierData[] = {
      {MockKeyboard::NONE,       "false,false,false"},
      {MockKeyboard::LEFT_SHIFT, "true,false,false"},
#if defined(OS_WIN)
      {MockKeyboard::RIGHT_ALT,  "false,false,true"},
#endif
    };

    MockKeyboard::Layout layout = kLayouts[i];
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kModifierData); ++j) {
      // Virtual key codes used for this test.
      static const int kKeyCodes[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z',
        ui::VKEY_OEM_1,
        ui::VKEY_OEM_PLUS,
        ui::VKEY_OEM_COMMA,
        ui::VKEY_OEM_MINUS,
        ui::VKEY_OEM_PERIOD,
        ui::VKEY_OEM_2,
        ui::VKEY_OEM_3,
        ui::VKEY_OEM_4,
        ui::VKEY_OEM_5,
        ui::VKEY_OEM_6,
        ui::VKEY_OEM_7,
#if defined(OS_WIN)
        // Not sure how to handle this key on Linux.
        ui::VKEY_OEM_8,
#endif
      };

      MockKeyboard::Modifiers modifiers = kModifierData[j].modifiers;
      for (size_t k = 0; k < ARRAYSIZE_UNSAFE(kKeyCodes); ++k) {
        // Send a keyboard event to the RenderView object.
        // We should test a keyboard event only when the given keyboard-layout
        // driver is installed in a PC and the driver can assign a Unicode
        // charcter for the given tuple (key-code and modifiers).
        int key_code = kKeyCodes[k];
        string16 char_code;
        if (SendKeyEvent(layout, key_code, modifiers, &char_code) < 0)
          continue;

        // Create an expected result from the virtual-key code, the character
        // code, and the modifier-key status.
        // We format a string that emulates a DOM-event string produced hy
        // our JavaScript function. (See the above comment for the format.)
        static char expected_result[1024];
        expected_result[0] = 0;
        base::snprintf(&expected_result[0],
                       sizeof(expected_result),
                       "\n"       // texts in the <input> element
                       "%d,%s\n"  // texts in the first <div> element
                       "%d,%s\n"  // texts in the second <div> element
                       "%d,%s",   // texts in the third <div> element
                       key_code, kModifierData[j].expected_result,
                       static_cast<int>(char_code[0]),
                       kModifierData[j].expected_result,
                       key_code, kModifierData[j].expected_result);

        // Retrieve the text in the test page and compare it with the expected
        // text created from a virtual-key code, a character code, and the
        // modifier-key status.
        const int kMaxOutputCharacters = 1024;
        std::string output = UTF16ToUTF8(
            GetMainFrame()->contentAsText(kMaxOutputCharacters));
        EXPECT_EQ(expected_result, output);
      }
    }
  }
#else
  NOTIMPLEMENTED();
#endif
}

// Test that our EditorClientImpl class can insert characters when we send
// keyboard events through the RenderWidget::OnHandleInputEvent() function.
// This test is for preventing regressions caused only when we use non-US
// keyboards, such as Issue 10846.
TEST_F(RenderViewImplTest, InsertCharacters) {
#if !defined(OS_MACOSX)
  static const struct {
    MockKeyboard::Layout layout;
    const wchar_t* expected_result;
  } kLayouts[] = {
#if 0
    // Disabled these keyboard layouts because buildbots do not have their
    // keyboard-layout drivers installed.
    {MockKeyboard::LAYOUT_ARABIC,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0634\x0624\x064a\x062b\x0628\x0644"
     L"\x0627\x0647\x062a\x0646\x0645\x0629\x0649\x062e"
     L"\x062d\x0636\x0642\x0633\x0641\x0639\x0631\x0635"
     L"\x0621\x063a\x0626\x0643\x003d\x0648\x002d\x0632"
     L"\x0638\x0630\x062c\x005c\x062f\x0637\x0028\x0021"
     L"\x0040\x0023\x0024\x0025\x005e\x0026\x002a\x0029"
     L"\x0650\x007d\x005d\x064f\x005b\x0623\x00f7\x0640"
     L"\x060c\x002f\x2019\x0622\x00d7\x061b\x064e\x064c"
     L"\x064d\x2018\x007b\x064b\x0652\x0625\x007e\x003a"
     L"\x002b\x002c\x005f\x002e\x061f\x0651\x003c\x007c"
     L"\x003e\x0022\x0030\x0031\x0032\x0033\x0034\x0035"
     L"\x0036\x0037\x0038\x0039\x0634\x0624\x064a\x062b"
     L"\x0628\x0644\x0627\x0647\x062a\x0646\x0645\x0629"
     L"\x0649\x062e\x062d\x0636\x0642\x0633\x0641\x0639"
     L"\x0631\x0635\x0621\x063a\x0626\x0643\x003d\x0648"
     L"\x002d\x0632\x0638\x0630\x062c\x005c\x062f\x0637"
    },
    {MockKeyboard::LAYOUT_HEBREW,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x05e9\x05e0\x05d1\x05d2\x05e7\x05db"
     L"\x05e2\x05d9\x05df\x05d7\x05dc\x05da\x05e6\x05de"
     L"\x05dd\x05e4\x002f\x05e8\x05d3\x05d0\x05d5\x05d4"
     L"\x0027\x05e1\x05d8\x05d6\x05e3\x003d\x05ea\x002d"
     L"\x05e5\x002e\x003b\x005d\x005c\x005b\x002c\x0028"
     L"\x0021\x0040\x0023\x0024\x0025\x005e\x0026\x002a"
     L"\x0029\x0041\x0042\x0043\x0044\x0045\x0046\x0047"
     L"\x0048\x0049\x004a\x004b\x004c\x004d\x004e\x004f"
     L"\x0050\x0051\x0052\x0053\x0054\x0055\x0056\x0057"
     L"\x0058\x0059\x005a\x003a\x002b\x003e\x005f\x003c"
     L"\x003f\x007e\x007d\x007c\x007b\x0022\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x05e9\x05e0\x05d1\x05d2\x05e7\x05db\x05e2\x05d9"
     L"\x05df\x05d7\x05dc\x05da\x05e6\x05de\x05dd\x05e4"
     L"\x002f\x05e8\x05d3\x05d0\x05d5\x05d4\x0027\x05e1"
     L"\x05d8\x05d6\x05e3\x003d\x05ea\x002d\x05e5\x002e"
     L"\x003b\x005d\x005c\x005b\x002c"
    },
#endif
#if defined(OS_WIN)
    // On Linux, the only way to test alternate keyboard layouts is to change
    // the keyboard layout of the whole screen. I'm worried about the side
    // effects this may have on the buildbots.
    {MockKeyboard::LAYOUT_CANADIAN_FRENCH,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x00e9\x003c\x0029\x0021\x0022\x002f\x0024"
     L"\x0025\x003f\x0026\x002a\x0028\x0041\x0042\x0043"
     L"\x0044\x0045\x0046\x0047\x0048\x0049\x004a\x004b"
     L"\x004c\x004d\x004e\x004f\x0050\x0051\x0052\x0053"
     L"\x0054\x0055\x0056\x0057\x0058\x0059\x005a\x003a"
     L"\x002b\x0027\x005f\x002e\x00c9\x003e\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x0061\x0062\x0063\x0064\x0065\x0066\x0067\x0068"
     L"\x0069\x006a\x006b\x006c\x006d\x006e\x006f\x0070"
     L"\x0071\x0072\x0073\x0074\x0075\x0076\x0077\x0078"
     L"\x0079\x007a\x003b\x003d\x002c\x002d\x002e\x00e9"
     L"\x003c"
    },
    {MockKeyboard::LAYOUT_FRENCH,
     L"\x00e0\x0026\x00e9\x0022\x0027\x0028\x002d\x00e8"
     L"\x005f\x00e7\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x0024\x003d\x002c\x003b"
     L"\x003a\x00f9\x0029\x002a\x0021\x0030\x0031\x0032"
     L"\x0033\x0034\x0035\x0036\x0037\x0038\x0039\x0041"
     L"\x0042\x0043\x0044\x0045\x0046\x0047\x0048\x0049"
     L"\x004a\x004b\x004c\x004d\x004e\x004f\x0050\x0051"
     L"\x0052\x0053\x0054\x0055\x0056\x0057\x0058\x0059"
     L"\x005a\x00a3\x002b\x003f\x002e\x002f\x0025\x00b0"
     L"\x00b5\x00e0\x0026\x00e9\x0022\x0027\x0028\x002d"
     L"\x00e8\x005f\x00e7\x0061\x0062\x0063\x0064\x0065"
     L"\x0066\x0067\x0068\x0069\x006a\x006b\x006c\x006d"
     L"\x006e\x006f\x0070\x0071\x0072\x0073\x0074\x0075"
     L"\x0076\x0077\x0078\x0079\x007a\x0024\x003d\x002c"
     L"\x003b\x003a\x00f9\x0029\x002a\x0021"
    },
    {MockKeyboard::LAYOUT_RUSSIAN,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0444\x0438\x0441\x0432\x0443\x0430"
     L"\x043f\x0440\x0448\x043e\x043b\x0434\x044c\x0442"
     L"\x0449\x0437\x0439\x043a\x044b\x0435\x0433\x043c"
     L"\x0446\x0447\x043d\x044f\x0436\x003d\x0431\x002d"
     L"\x044e\x002e\x0451\x0445\x005c\x044a\x044d\x0029"
     L"\x0021\x0022\x2116\x003b\x0025\x003a\x003f\x002a"
     L"\x0028\x0424\x0418\x0421\x0412\x0423\x0410\x041f"
     L"\x0420\x0428\x041e\x041b\x0414\x042c\x0422\x0429"
     L"\x0417\x0419\x041a\x042b\x0415\x0413\x041c\x0426"
     L"\x0427\x041d\x042f\x0416\x002b\x0411\x005f\x042e"
     L"\x002c\x0401\x0425\x002f\x042a\x042d\x0030\x0031"
     L"\x0032\x0033\x0034\x0035\x0036\x0037\x0038\x0039"
     L"\x0444\x0438\x0441\x0432\x0443\x0430\x043f\x0440"
     L"\x0448\x043e\x043b\x0434\x044c\x0442\x0449\x0437"
     L"\x0439\x043a\x044b\x0435\x0433\x043c\x0446\x0447"
     L"\x043d\x044f\x0436\x003d\x0431\x002d\x044e\x002e"
     L"\x0451\x0445\x005c\x044a\x044d"
    },
#endif  // defined(OS_WIN)
    {MockKeyboard::LAYOUT_UNITED_STATES,
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x002f\x0060\x005b\x005c\x005d\x0027\x0029"
     L"\x0021\x0040\x0023\x0024\x0025\x005e\x0026\x002a"
     L"\x0028\x0041\x0042\x0043\x0044\x0045\x0046\x0047"
     L"\x0048\x0049\x004a\x004b\x004c\x004d\x004e\x004f"
     L"\x0050\x0051\x0052\x0053\x0054\x0055\x0056\x0057"
     L"\x0058\x0059\x005a\x003a\x002b\x003c\x005f\x003e"
     L"\x003f\x007e\x007b\x007c\x007d\x0022"
#if defined(OS_WIN)
     // This is ifdefed out for Linux to correspond to the fact that we don't
     // test alt+keystroke for now.
     L"\x0030\x0031\x0032\x0033\x0034\x0035\x0036\x0037"
     L"\x0038\x0039\x0061\x0062\x0063\x0064\x0065\x0066"
     L"\x0067\x0068\x0069\x006a\x006b\x006c\x006d\x006e"
     L"\x006f\x0070\x0071\x0072\x0073\x0074\x0075\x0076"
     L"\x0077\x0078\x0079\x007a\x003b\x003d\x002c\x002d"
     L"\x002e\x002f\x0060\x005b\x005c\x005d\x0027"
#endif
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kLayouts); ++i) {
    // Load an HTML page consisting of one <div> element.
    // This <div> element is used by the EditorClientImpl class to insert
    // characters received through the RenderWidget::OnHandleInputEvent()
    // function.
    view()->set_send_content_state_immediately(true);
    LoadHTML("<html>"
             "<head>"
             "<title></title>"
             "</head>"
             "<body>"
             "<div id='test' contenteditable='true'>"
             "</div>"
             "</body>"
             "</html>");
    ExecuteJavaScript("document.getElementById('test').focus();");
    render_thread_->sink().ClearMessages();

    // For each key code, we send three keyboard events.
    //  * Pressing only the key;
    //  * Pressing the key and a left-shift key, and;
    //  * Pressing the key and a right-alt (AltGr) key.
    static const MockKeyboard::Modifiers kModifiers[] = {
      MockKeyboard::NONE,
      MockKeyboard::LEFT_SHIFT,
#if defined(OS_WIN)
      MockKeyboard::RIGHT_ALT,
#endif
    };

    MockKeyboard::Layout layout = kLayouts[i].layout;
    for (size_t j = 0; j < ARRAYSIZE_UNSAFE(kModifiers); ++j) {
      // Virtual key codes used for this test.
      static const int kKeyCodes[] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
        'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
        'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V',
        'W', 'X', 'Y', 'Z',
        ui::VKEY_OEM_1,
        ui::VKEY_OEM_PLUS,
        ui::VKEY_OEM_COMMA,
        ui::VKEY_OEM_MINUS,
        ui::VKEY_OEM_PERIOD,
        ui::VKEY_OEM_2,
        ui::VKEY_OEM_3,
        ui::VKEY_OEM_4,
        ui::VKEY_OEM_5,
        ui::VKEY_OEM_6,
        ui::VKEY_OEM_7,
#if defined(OS_WIN)
        // Unclear how to handle this on Linux.
        ui::VKEY_OEM_8,
#endif
      };

      MockKeyboard::Modifiers modifiers = kModifiers[j];
      for (size_t k = 0; k < ARRAYSIZE_UNSAFE(kKeyCodes); ++k) {
        // Send a keyboard event to the RenderView object.
        // We should test a keyboard event only when the given keyboard-layout
        // driver is installed in a PC and the driver can assign a Unicode
        // charcter for the given tuple (layout, key-code, and modifiers).
        int key_code = kKeyCodes[k];
        string16 char_code;
        if (SendKeyEvent(layout, key_code, modifiers, &char_code) < 0)
          continue;
      }
    }

    // Retrieve the text in the test page and compare it with the expected
    // text created from a virtual-key code, a character code, and the
    // modifier-key status.
    const int kMaxOutputCharacters = 4096;
    std::wstring output = UTF16ToWideHack(
        GetMainFrame()->contentAsText(kMaxOutputCharacters));
    EXPECT_EQ(kLayouts[i].expected_result, output);
  }
#else
  NOTIMPLEMENTED();
#endif
}

// Crashy, http://crbug.com/53247.
TEST_F(RenderViewImplTest, DISABLED_DidFailProvisionalLoadWithErrorForError) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_FILE_NOT_FOUND;
  error.unreachableURL = GURL("http://foo");
  WebFrame* web_frame = GetMainFrame();
  // An error occurred.
  view()->didFailProvisionalLoad(web_frame, error);
  // Frame should exit view-source mode.
  EXPECT_FALSE(web_frame->isViewSourceModeEnabled());
}

TEST_F(RenderViewImplTest, DidFailProvisionalLoadWithErrorForCancellation) {
  GetMainFrame()->enableViewSourceMode(true);
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = GURL("http://foo");
  WebFrame* web_frame = GetMainFrame();
  // A cancellation occurred.
  view()->didFailProvisionalLoad(web_frame, error);
  // Frame should stay in view-source mode.
  EXPECT_TRUE(web_frame->isViewSourceModeEnabled());
}

// Regression test for http://crbug.com/41562
TEST_F(RenderViewImplTest, UpdateTargetURLWithInvalidURL) {
  const GURL invalid_gurl("http://");
  view()->setMouseOverURL(WebKit::WebURL(invalid_gurl));
  EXPECT_EQ(invalid_gurl, view()->target_url_);
}

TEST_F(RenderViewImplTest, SetHistoryLengthAndPrune) {
  int expected_page_id = -1;

  // No history to merge and no committed pages.
  view()->OnSetHistoryLengthAndPrune(0, -1);
  EXPECT_EQ(0, view()->history_list_length_);
  EXPECT_EQ(-1, view()->history_list_offset_);

  // History to merge and no committed pages.
  view()->OnSetHistoryLengthAndPrune(2, -1);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  ClearHistory();

  // No history to merge and a committed page to be kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(0, expected_page_id);
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[0]);
  ClearHistory();

  // No history to merge and a committed page to be pruned.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(0, expected_page_id + 1);
  EXPECT_EQ(0, view()->history_list_length_);
  EXPECT_EQ(-1, view()->history_list_offset_);
  ClearHistory();

  // No history to merge and a committed page that the browser was unaware of.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(0, -1);
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[0]);
  ClearHistory();

  // History to merge and a committed page to be kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(2, expected_page_id);
  EXPECT_EQ(3, view()->history_list_length_);
  EXPECT_EQ(2, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[2]);
  ClearHistory();

  // History to merge and a committed page to be pruned.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(2, expected_page_id + 1);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  ClearHistory();

  // History to merge and a committed page that the browser was unaware of.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->OnSetHistoryLengthAndPrune(2, -1);
  EXPECT_EQ(3, view()->history_list_length_);
  EXPECT_EQ(2, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[2]);
  ClearHistory();

  int expected_page_id_2 = -1;

  // No history to merge and two committed pages, both to be kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(0, expected_page_id);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[0]);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[1]);
  ClearHistory();

  // No history to merge and two committed pages, and only the second is kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(0, expected_page_id_2);
  EXPECT_EQ(1, view()->history_list_length_);
  EXPECT_EQ(0, view()->history_list_offset_);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[0]);
  ClearHistory();

  // No history to merge and two committed pages, both of which the browser was
  // unaware of.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(0, -1);
  EXPECT_EQ(2, view()->history_list_length_);
  EXPECT_EQ(1, view()->history_list_offset_);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[0]);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[1]);
  ClearHistory();

  // History to merge and two committed pages, both to be kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(2, expected_page_id);
  EXPECT_EQ(4, view()->history_list_length_);
  EXPECT_EQ(3, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[2]);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[3]);
  ClearHistory();

  // History to merge and two committed pages, and only the second is kept.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(2, expected_page_id_2);
  EXPECT_EQ(3, view()->history_list_length_);
  EXPECT_EQ(2, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[2]);
  ClearHistory();

  // History to merge and two committed pages, both of which the browser was
  // unaware of.
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id = view()->page_id_;
  view()->didCommitProvisionalLoad(GetMainFrame(), true);
  expected_page_id_2 = view()->page_id_;
  EXPECT_GT(expected_page_id_2, expected_page_id);
  view()->OnSetHistoryLengthAndPrune(2, -1);
  EXPECT_EQ(4, view()->history_list_length_);
  EXPECT_EQ(3, view()->history_list_offset_);
  EXPECT_EQ(-1, view()->history_page_ids_[0]);
  EXPECT_EQ(-1, view()->history_page_ids_[1]);
  EXPECT_EQ(expected_page_id, view()->history_page_ids_[2]);
  EXPECT_EQ(expected_page_id_2, view()->history_page_ids_[3]);
}

TEST_F(RenderViewImplTest, FindTitleForIntentsPage) {
  view()->set_send_content_state_immediately(true);
  LoadHTML("<html><head><title>title</title>"
           "<intent action=\"a\" type=\"t\"></intent></head></html>");
  WebKit::WebIntentServiceInfo service;
  service.setAction(ASCIIToUTF16("a"));
  service.setType(ASCIIToUTF16("t"));
  view()->registerIntentService(GetMainFrame(), service);
  ProcessPendingMessages();

  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      IntentsHostMsg_RegisterIntentService::ID));
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      IntentsHostMsg_RegisterIntentService::ID);
  ASSERT_TRUE(msg);
  webkit_glue::WebIntentServiceData service_data;
  bool user_gesture = true;
  IntentsHostMsg_RegisterIntentService::Read(msg, &service_data, &user_gesture);
  EXPECT_EQ(ASCIIToUTF16("a"), service_data.action);
  EXPECT_EQ(ASCIIToUTF16("t"), service_data.type);
  EXPECT_EQ(ASCIIToUTF16("title"), service_data.title);
  EXPECT_FALSE(user_gesture);
}

TEST_F(RenderViewImplTest, ContextMenu) {
  LoadHTML("<div>Page A</div>");

  // Create a right click in the center of the iframe. (I'm hoping this will
  // make this a bit more robust in case of some other formatting or other bug.)
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MouseDown;
  mouse_event.button = WebMouseEvent::ButtonRight;
  mouse_event.x = 250;
  mouse_event.y = 250;
  mouse_event.globalX = 250;
  mouse_event.globalY = 250;

  SendWebMouseEvent(mouse_event);

  // Now simulate the corresponding up event which should display the menu
  mouse_event.type = WebInputEvent::MouseUp;
  SendWebMouseEvent(mouse_event);

  EXPECT_TRUE(render_thread_->sink().GetUniqueMessageMatching(
      ViewHostMsg_ContextMenu::ID));
}

TEST_F(RenderViewImplTest, TestBackForward) {
  LoadHTML("<div id=pagename>Page A</div>");
  WebKit::WebHistoryItem page_a_item = GetMainFrame()->currentHistoryItem();
  int was_page_a = -1;
  string16 check_page_a =
      ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page A')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  LoadHTML("<div id=pagename>Page B</div>");
  int was_page_b = -1;
  string16 check_page_b =
      ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page B')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  LoadHTML("<div id=pagename>Page C</div>");
  int was_page_c = -1;
  string16 check_page_c =
      ASCIIToUTF16(
          "Number(document.getElementById('pagename').innerHTML == 'Page C')");
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_b);

  WebKit::WebHistoryItem forward_item = GetMainFrame()->currentHistoryItem();
  GoBack(GetMainFrame()->previousHistoryItem());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  GoForward(forward_item);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_c, &was_page_c));
  EXPECT_EQ(1, was_page_c);

  GoBack(GetMainFrame()->previousHistoryItem());
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);

  forward_item = GetMainFrame()->currentHistoryItem();
  GoBack(page_a_item);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_a, &was_page_a));
  EXPECT_EQ(1, was_page_a);

  GoForward(forward_item);
  EXPECT_TRUE(ExecuteJavaScriptAndReturnIntValue(check_page_b, &was_page_b));
  EXPECT_EQ(1, was_page_b);
}

TEST_F(RenderViewImplTest, GetCompositionCharacterBoundsTest) {
  LoadHTML("<textarea id=\"test\"></textarea>");
  ExecuteJavaScript("document.getElementById('test').focus();");

  const string16 empty_string = UTF8ToUTF16("");
  const std::vector<WebKit::WebCompositionUnderline> empty_underline;
  std::vector<gfx::Rect> bounds;
  view()->OnSetFocus(true);
  view()->OnSetInputMethodActive(true);

  // ASCII composition
  const string16 ascii_composition = UTF8ToUTF16("aiueo");
  view()->OnImeSetComposition(ascii_composition, empty_underline, 0, 0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(ascii_composition.size(), bounds.size());
  for (size_t i = 0; i < bounds.size(); ++i)
    EXPECT_LT(0, bounds[i].width());
  view()->OnImeConfirmComposition(empty_string, ui::Range::InvalidRange());

  // Non surrogate pair unicode character.
  const string16 unicode_composition = UTF8ToUTF16(
      "\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A");
  view()->OnImeSetComposition(unicode_composition, empty_underline, 0, 0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(unicode_composition.size(), bounds.size());
  for (size_t i = 0; i < bounds.size(); ++i)
    EXPECT_LT(0, bounds[i].width());
  view()->OnImeConfirmComposition(empty_string, ui::Range::InvalidRange());

  // Surrogate pair character.
  const string16 surrogate_pair_char = UTF8ToUTF16("\xF0\xA0\xAE\x9F");
  view()->OnImeSetComposition(surrogate_pair_char,
                              empty_underline,
                              0,
                              0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(surrogate_pair_char.size(), bounds.size());
  EXPECT_LT(0, bounds[0].width());
  EXPECT_EQ(0, bounds[1].width());
  view()->OnImeConfirmComposition(empty_string, ui::Range::InvalidRange());

  // Mixed string.
  const string16 surrogate_pair_mixed_composition =
      surrogate_pair_char + UTF8ToUTF16("\xE3\x81\x82") + surrogate_pair_char +
      UTF8ToUTF16("b") + surrogate_pair_char;
  const size_t utf16_length = 8UL;
  const bool is_surrogate_pair_empty_rect[8] = {
    false, true, false, false, true, false, false, true };
  view()->OnImeSetComposition(surrogate_pair_mixed_composition,
                              empty_underline,
                              0,
                              0);
  view()->GetCompositionCharacterBounds(&bounds);
  ASSERT_EQ(utf16_length, bounds.size());
  for (size_t i = 0; i < utf16_length; ++i) {
    if (is_surrogate_pair_empty_rect[i]) {
      EXPECT_EQ(0, bounds[i].width());
    } else {
      EXPECT_LT(0, bounds[i].width());
    }
  }
  view()->OnImeConfirmComposition(empty_string, ui::Range::InvalidRange());
}

TEST_F(RenderViewImplTest, ZoomLimit) {
  const double kMinZoomLevel =
      WebKit::WebView::zoomFactorToZoomLevel(kMinimumZoomFactor);
  const double kMaxZoomLevel =
      WebKit::WebView::zoomFactorToZoomLevel(kMaximumZoomFactor);

  ViewMsg_Navigate_Params params;
  params.page_id = -1;
  params.navigation_type = ViewMsg_Navigate_Type::NORMAL;

  // Verifies navigation to a URL with preset zoom level indeed sets the level.
  // Regression test for http://crbug.com/139559, where the level was not
  // properly set when it is out of the default zoom limits of WebView.
  params.url = GURL("data:text/html,min_zoomlimit_test");
  view()->OnSetZoomLevelForLoadingURL(params.url, kMinZoomLevel);
  view()->OnNavigate(params);
  ProcessPendingMessages();
  EXPECT_DOUBLE_EQ(kMinZoomLevel, view()->GetWebView()->zoomLevel());

  // It should work even when the zoom limit is temporarily changed in the page.
  view()->GetWebView()->zoomLimitsChanged(
      WebKit::WebView::zoomFactorToZoomLevel(1.0),
      WebKit::WebView::zoomFactorToZoomLevel(1.0));
  params.url = GURL("data:text/html,max_zoomlimit_test");
  view()->OnSetZoomLevelForLoadingURL(params.url, kMaxZoomLevel);
  view()->OnNavigate(params);
  ProcessPendingMessages();
  EXPECT_DOUBLE_EQ(kMaxZoomLevel, view()->GetWebView()->zoomLevel());
}

TEST_F(RenderViewImplTest, SetEditableSelectionAndComposition) {
  // Load an HTML page consisting of an input field.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" value=\"some test text hello\"></input>"
           "</body>"
           "</html>");
  ExecuteJavaScript("document.getElementById('test1').focus();");
  view()->OnSetEditableSelectionOffsets(4, 8);
  const std::vector<WebKit::WebCompositionUnderline> empty_underline;
  view()->OnSetCompositionFromExistingText(7,10, empty_underline);
  WebKit::WebTextInputInfo info = view()->webview()->textInputInfo();
  EXPECT_EQ(4, info.selectionStart);
  EXPECT_EQ(8, info.selectionEnd);
  EXPECT_EQ(7, info.compositionStart);
  EXPECT_EQ(10, info.compositionEnd);
  view()->OnUnselect();
  info = view()->webview()->textInputInfo();
  EXPECT_EQ(0, info.selectionStart);
  EXPECT_EQ(0, info.selectionEnd);
}


TEST_F(RenderViewImplTest, OnExtendSelectionAndDelete) {
  // Load an HTML page consisting of an input field.
  LoadHTML("<html>"
           "<head>"
           "</head>"
           "<body>"
           "<input id=\"test1\" value=\"abcdefghijklmnopqrstuvwxyz\"></input>"
           "</body>"
           "</html>");
  ExecuteJavaScript("document.getElementById('test1').focus();");
  view()->OnSetEditableSelectionOffsets(10, 10);
  view()->OnExtendSelectionAndDelete(3, 4);
  WebKit::WebTextInputInfo info = view()->webview()->textInputInfo();
  EXPECT_EQ("abcdefgopqrstuvwxyz", info.value);
  EXPECT_EQ(7, info.selectionStart);
  EXPECT_EQ(7, info.selectionEnd);
  view()->OnSetEditableSelectionOffsets(4, 8);
  view()->OnExtendSelectionAndDelete(2, 5);
  info = view()->webview()->textInputInfo();
  EXPECT_EQ("abuvwxyz", info.value);
  EXPECT_EQ(2, info.selectionStart);
  EXPECT_EQ(2, info.selectionEnd);
}

}  // namespace content
