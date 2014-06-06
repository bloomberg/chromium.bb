// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_EVENT_SENDER_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_EVENT_SENDER_H_

#include <queue>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/shell/renderer/test_runner/WebTask.h"
#include "third_party/WebKit/public/platform/WebDragData.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebTouchPoint.h"

namespace blink {
class WebFrame;
class WebView;
struct WebContextMenuData;
}

namespace gin {
class Arguments;
}

namespace content {

class TestInterfaces;
class WebTestDelegate;

// Key event location code introduced in DOM Level 3.
// See also: http://www.w3.org/TR/DOM-Level-3-Events/#events-keyboardevents
enum KeyLocationCode {
  DOMKeyLocationStandard      = 0x00,
  DOMKeyLocationLeft          = 0x01,
  DOMKeyLocationRight         = 0x02,
  DOMKeyLocationNumpad        = 0x03
};

class EventSender : public base::SupportsWeakPtr<EventSender> {
 public:
  explicit EventSender(TestInterfaces*);
  virtual ~EventSender();

  void Reset();
  void Install(blink::WebFrame*);
  void SetDelegate(WebTestDelegate*);
  void SetWebView(blink::WebView*);

  void SetContextMenuData(const blink::WebContextMenuData&);

  void DoDragDrop(const blink::WebDragData&, blink::WebDragOperationsMask);

  void MouseDown(int button_number, int modifiers);
  void MouseUp(int button_number, int modifiers);
  void KeyDown(const std::string& code_str,
               int modifiers,
               KeyLocationCode location);

  WebTaskList* mutable_task_list() { return &task_list_; }

 private:
  friend class EventSenderBindings;

  struct SavedEvent {
    enum SavedEventType {
      TYPE_UNSPECIFIED,
      TYPE_MOUSE_UP,
      TYPE_MOUSE_MOVE,
      TYPE_LEAP_FORWARD
    };

    SavedEvent();

    SavedEventType type;
    blink::WebMouseEvent::Button button_type;  // For MouseUp.
    blink::WebPoint pos;                       // For MouseMove.
    int milliseconds;                          // For LeapForward.
    int modifiers;
  };

  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();

  std::vector<std::string> ContextClick();

  void TextZoomIn();
  void TextZoomOut();

  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double zoom_factor);

  void SetPageScaleFactor(float scale_factor, int x, int y);

  void ClearTouchPoints();
  void ReleaseTouchPoint(unsigned index);
  void UpdateTouchPoint(unsigned index, float x, float y);
  void CancelTouchPoint(unsigned index);
  void SetTouchModifier(const std::string& key_name, bool set_mask);
  void SetTouchCancelable(bool cancelable);
  void ThrowTouchPointError();

  void DumpFilenameBeingDragged();

  void GestureFlingCancel();
  void GestureFlingStart(float x, float y, float velocity_x, float velocity_y);
  void GestureScrollFirstPoint(int x, int y);

  void TouchStart();
  void TouchMove();
  void TouchCancel();
  void TouchEnd();

  void LeapForward(int milliseconds);

  void BeginDragWithFiles(const std::vector<std::string>& files);

  void AddTouchPoint(gin::Arguments* args);

  void MouseDragBegin();
  void MouseDragEnd();

  void GestureScrollBegin(gin::Arguments* args);
  void GestureScrollEnd(gin::Arguments* args);
  void GestureScrollUpdate(gin::Arguments* args);
  void GestureScrollUpdateWithoutPropagation(gin::Arguments* args);
  void GestureTap(gin::Arguments* args);
  void GestureTapDown(gin::Arguments* args);
  void GestureShowPress(gin::Arguments* args);
  void GestureTapCancel(gin::Arguments* args);
  void GestureLongPress(gin::Arguments* args);
  void GestureLongTap(gin::Arguments* args);
  void GestureTwoFingerTap(gin::Arguments* args);

  void ContinuousMouseScrollBy(gin::Arguments* args);
  void MouseMoveTo(gin::Arguments* args);
  void TrackpadScrollBegin();
  void TrackpadScroll(gin::Arguments* args);
  void TrackpadScrollEnd();
  void MouseScrollBy(gin::Arguments* args);
  void MouseMomentumBegin();
  void MouseMomentumBegin2(gin::Arguments* args);
  void MouseMomentumScrollBy(gin::Arguments* args);
  void MouseMomentumEnd();
  void ScheduleAsynchronousClick(int button_number, int modifiers);
  void ScheduleAsynchronousKeyDown(const std::string& code_str,
                                   int modifiers,
                                   KeyLocationCode location);

  double GetCurrentEventTimeSec();

  void DoLeapForward(int milliseconds);

  void SendCurrentTouchEvent(blink::WebInputEvent::Type);

  void GestureEvent(blink::WebInputEvent::Type, gin::Arguments*);

  void UpdateClickCountForButton(blink::WebMouseEvent::Button);

  void InitMouseWheelEvent(gin::Arguments* args,
                           bool continuous,
                           blink::WebMouseWheelEvent* event);

  void FinishDragAndDrop(const blink::WebMouseEvent&, blink::WebDragOperation);

  void DoMouseUp(const blink::WebMouseEvent&);
  void DoMouseMove(const blink::WebMouseEvent&);
  void ReplaySavedEvents();

  bool force_layout_on_events() const { return force_layout_on_events_; }
  void set_force_layout_on_events(bool force) {
    force_layout_on_events_ = force;
  }

  bool is_drag_mode() const { return is_drag_mode_; }
  void set_is_drag_mode(bool drag_mode) { is_drag_mode_ = drag_mode; }

#if defined(OS_WIN)
  int wm_key_down() const { return wm_key_down_; }
  void set_wm_key_down(int key_down) { wm_key_down_ = key_down; }

  int wm_key_up() const { return wm_key_up_; }
  void set_wm_key_up(int key_up) { wm_key_up_ = key_up; }

  int wm_char() const { return wm_char_; }
  void set_wm_char(int wm_char) { wm_char_ = wm_char; }

  int wm_dead_char() const { return wm_dead_char_; }
  void set_wm_dead_char(int dead_char) {
    wm_dead_char_ = dead_char;
  }

  int wm_sys_key_down() const { return wm_sys_key_down_; }
  void set_wm_sys_key_down(int key_down) { wm_sys_key_down_ = key_down; }

  int wm_sys_key_up() const { return wm_sys_key_up_; }
  void set_wm_sys_key_up(int key_up) { wm_sys_key_up_ = key_up; }

  int wm_sys_char() const { return wm_sys_char_; }
  void set_wm_sys_char(int sys_char) { wm_sys_char_ = sys_char; }

  int wm_sys_dead_char() const { return wm_sys_dead_char_; }
  void set_wm_sys_dead_char(int sys_dead_char) {
    wm_sys_dead_char_ = sys_dead_char;
  }

  int wm_key_down_;
  int wm_key_up_;
  int wm_char_;
  int wm_dead_char_;
  int wm_sys_key_down_;
  int wm_sys_key_up_;
  int wm_sys_char_;
  int wm_sys_dead_char_;
#endif

  WebTaskList task_list_;

  TestInterfaces* interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* view_;

  bool force_layout_on_events_;

  // When set to true (the default value), we batch mouse move and mouse up
  // events so we can simulate drag & drop.
  bool is_drag_mode_;

  int touch_modifiers_;
  bool touch_cancelable_;
  std::vector<blink::WebTouchPoint> touch_points_;

  scoped_ptr<blink::WebContextMenuData> last_context_menu_data_;

  blink::WebDragData current_drag_data_;

  // Location of the touch point that initiated a gesture.
  blink::WebPoint current_gesture_location_;

  // Currently pressed mouse button (Left/Right/Middle or None).
  static blink::WebMouseEvent::Button pressed_button_;

  bool replaying_saved_events_;

  std::deque<SavedEvent> mouse_event_queue_;

  blink::WebDragOperationsMask current_drag_effects_allowed_;

  // Location of last mouseMoveTo event.
  static blink::WebPoint last_mouse_pos_;

  // Time and place of the last mouse up event.
  double last_click_time_sec_;
  blink::WebPoint last_click_pos_;

  // The last button number passed to mouseDown and mouseUp.
  // Used to determine whether the click count continues to increment or not.
  static blink::WebMouseEvent::Button last_button_type_;

  blink::WebDragOperation current_drag_effect_;

  uint32 time_offset_ms_;
  int click_count_;

  base::WeakPtrFactory<EventSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(EventSender);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_EVENT_SENDER_H_
