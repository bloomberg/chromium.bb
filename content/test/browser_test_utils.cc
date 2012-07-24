// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "base/json/json_reader.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class DOMOperationObserver : public NotificationObserver,
                             public WebContentsObserver {
 public:
  explicit DOMOperationObserver(RenderViewHost* render_view_host)
      : WebContentsObserver(WebContents::FromRenderViewHost(render_view_host)),
        did_respond_(false) {
    registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                   Source<RenderViewHost>(render_view_host));
    message_loop_runner_ = new MessageLoopRunner;
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    DCHECK(type == NOTIFICATION_DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json;
    did_respond_ = true;
    message_loop_runner_->Quit();
  }

  // Overridden from WebContentsObserver:
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE {
    message_loop_runner_->Quit();
  }

  bool WaitAndGetResponse(std::string* response) WARN_UNUSED_RESULT {
    message_loop_runner_->Run();
    *response = response_;
    return did_respond_;
  }

 private:
  NotificationRegistrar registrar_;
  std::string response_;
  bool did_respond_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::wstring& frame_xpath,
                             const std::wstring& original_script,
                             scoped_ptr<Value>* result) WARN_UNUSED_RESULT;

// Executes the passed |original_script| in the frame pointed to by
// |frame_xpath|.  If |result| is not NULL, stores the value that the evaluation
// of the script in |result|.  Returns true on success.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::wstring& frame_xpath,
                             const std::wstring& original_script,
                             scoped_ptr<Value>* result) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::wstring script = L"window.domAutomationController.setAutomationId(0);" +
      original_script;
  DOMOperationObserver dom_op_observer(render_view_host);
  render_view_host->ExecuteJavascriptInWebFrame(WideToUTF16Hack(frame_xpath),
                                                WideToUTF16Hack(script));
  std::string json;
  if (!dom_op_observer.WaitAndGetResponse(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMOperationObserver.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  result->reset(reader.ReadToValue(json));
  if (!result->get()) {
    DLOG(ERROR) << reader.GetErrorMessage();
    return false;
  }

  return true;
}

void BuildSimpleWebKeyEvent(WebKit::WebInputEvent::Type type,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command,
                            NativeWebKeyboardEvent* event) {
  event->nativeKeyCode = 0;
  event->windowsKeyCode = key;
  event->setKeyIdentifierFromWindowsKeyCode();
  event->type = type;
  event->modifiers = 0;
  event->isSystemKey = false;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;

  if (type == WebKit::WebInputEvent::Char ||
      type == WebKit::WebInputEvent::RawKeyDown) {
    event->text[0] = key;
    event->unmodifiedText[0] = key;
  }

  if (control)
    event->modifiers |= WebKit::WebInputEvent::ControlKey;

  if (shift)
    event->modifiers |= WebKit::WebInputEvent::ShiftKey;

  if (alt)
    event->modifiers |= WebKit::WebInputEvent::AltKey;

  if (command)
    event->modifiers |= WebKit::WebInputEvent::MetaKey;
}

}  // namespace

void SimulateMouseClick(WebContents* web_contents) {
  int x = web_contents->GetView()->GetContainerSize().width() / 2;
  int y = web_contents->GetView()->GetContainerSize().height() / 2;
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.x = x;
  mouse_event.y = y;
  // Mac needs globalX/globalY for events to plugins.
  gfx::Rect offset;
  web_contents->GetView()->GetContainerBounds(&offset);
  mouse_event.globalX = x + offset.x();
  mouse_event.globalY = y + offset.y();
  mouse_event.clickCount = 1;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        WebKit::WebInputEvent::Type type,
                        const gfx::Point& point) {
  WebKit::WebMouseEvent mouse_event;
  mouse_event.type = type;
  mouse_event.x = point.x();
  mouse_event.y = point.y();
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  NativeWebKeyboardEvent event_down;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::RawKeyDown, key, control, shift, alt, command,
      &event_down);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_down);

  NativeWebKeyboardEvent char_event;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::Char, key, control, shift, alt, command,
      &char_event);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(char_event);

  NativeWebKeyboardEvent event_up;
  BuildSimpleWebKeyEvent(
      WebKit::WebInputEvent::KeyUp, key, control, shift, alt, command,
      &event_up);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event_up);
}

bool ExecuteJavaScript(RenderViewHost* render_view_host,
                       const std::wstring& frame_xpath,
                       const std::wstring& original_script) {
  std::wstring script =
      original_script + L";window.domAutomationController.send(0);";
  return ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, NULL);
}

bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsInteger(result);
}

bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsBoolean(result);
}

bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsString(result);
}

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const string16& expected_title)
    : web_contents_(web_contents),
      expected_title_observed_(false),
      quit_loop_on_observation_(false) {
  EXPECT_TRUE(web_contents != NULL);
  expected_titles_.push_back(expected_title);
  notification_registrar_.Add(this,
                              NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED,
                              Source<WebContents>(web_contents));

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since the
  // NavigationEntry's title hasn't changed.
  notification_registrar_.Add(
      this,
      NOTIFICATION_LOAD_STOP,
      Source<NavigationController>(&web_contents->GetController()));
}

void TitleWatcher::AlsoWaitForTitle(const string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const string16& TitleWatcher::WaitAndGetTitle() {
  if (expected_title_observed_)
    return observed_title_;
  quit_loop_on_observation_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  return observed_title_;
}

void TitleWatcher::Observe(int type,
                           const NotificationSource& source,
                           const NotificationDetails& details) {
  if (type == NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED) {
    WebContents* source_contents = Source<WebContents>(source).ptr();
    ASSERT_EQ(web_contents_, source_contents);
  } else if (type == NOTIFICATION_LOAD_STOP) {
    NavigationController* controller =
        Source<NavigationController>(source).ptr();
    ASSERT_EQ(&web_contents_->GetController(), controller);
  } else {
    FAIL() << "Unexpected notification received.";
  }

  std::vector<string16>::const_iterator it =
      std::find(expected_titles_.begin(),
                expected_titles_.end(),
                web_contents_->GetTitle());
  if (it == expected_titles_.end())
    return;
  observed_title_ = *it;
  expected_title_observed_ = true;
  if (quit_loop_on_observation_) {
    // Only call Quit once, on first Observe:
    quit_loop_on_observation_ = false;
    message_loop_runner_->Quit();
  }
}

}  // namespace content
