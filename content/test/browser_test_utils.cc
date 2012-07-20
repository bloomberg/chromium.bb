// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

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
