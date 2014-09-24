// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/event_sender.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/public/common/page_zoom.h"
#include "content/shell/renderer/test_runner/mock_spell_check.h"
#include "content/shell/renderer/test_runner/test_interfaces.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "v8/include/v8.h"

using blink::WebContextMenuData;
using blink::WebDragData;
using blink::WebDragOperationsMask;
using blink::WebFloatPoint;
using blink::WebFrame;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMenuItemInfo;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebString;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

void InitMouseEvent(WebInputEvent::Type t,
                    WebMouseEvent::Button b,
                    const WebPoint& pos,
                    double time_stamp,
                    int click_count,
                    int modifiers,
                    WebMouseEvent* e) {
  e->type = t;
  e->button = b;
  e->modifiers = modifiers;
  e->x = pos.x;
  e->y = pos.y;
  e->globalX = pos.x;
  e->globalY = pos.y;
  e->timeStampSeconds = time_stamp;
  e->clickCount = click_count;
}

int GetKeyModifier(const std::string& modifier_name) {
  const char* characters = modifier_name.c_str();
  if (!strcmp(characters, "ctrlKey")
#ifndef __APPLE__
      || !strcmp(characters, "addSelectionKey")
#endif
      ) {
    return WebInputEvent::ControlKey;
  } else if (!strcmp(characters, "shiftKey") ||
             !strcmp(characters, "rangeSelectionKey")) {
    return WebInputEvent::ShiftKey;
  } else if (!strcmp(characters, "altKey")) {
    return WebInputEvent::AltKey;
#ifdef __APPLE__
  } else if (!strcmp(characters, "metaKey") ||
             !strcmp(characters, "addSelectionKey")) {
    return WebInputEvent::MetaKey;
#else
  } else if (!strcmp(characters, "metaKey")) {
    return WebInputEvent::MetaKey;
#endif
  } else if (!strcmp(characters, "autoRepeat")) {
    return WebInputEvent::IsAutoRepeat;
  } else if (!strcmp(characters, "copyKey")) {
#ifdef __APPLE__
    return WebInputEvent::AltKey;
#else
    return WebInputEvent::ControlKey;
#endif
  }

  return 0;
}

int GetKeyModifiers(const std::vector<std::string>& modifier_names) {
  int modifiers = 0;
  for (std::vector<std::string>::const_iterator it = modifier_names.begin();
       it != modifier_names.end(); ++it) {
    modifiers |= GetKeyModifier(*it);
  }
  return modifiers;
}

int GetKeyModifiersFromV8(v8::Handle<v8::Value> value) {
  std::vector<std::string> modifier_names;
  if (value->IsString()) {
    modifier_names.push_back(gin::V8ToString(value));
  } else if (value->IsArray()) {
    gin::Converter<std::vector<std::string> >::FromV8(
        NULL, value, &modifier_names);
  }
  return GetKeyModifiers(modifier_names);
}

// Maximum distance (in space and time) for a mouse click to register as a
// double or triple click.
const double kMultipleClickTimeSec = 1;
const int kMultipleClickRadiusPixels = 5;
const char kSubMenuDepthIdentifier[] = "_";
const char kSubMenuIdentifier[] = " >";
const char kSeparatorIdentifier[] = "---------";

bool OutsideMultiClickRadius(const WebPoint& a, const WebPoint& b) {
  return ((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)) >
         kMultipleClickRadiusPixels * kMultipleClickRadiusPixels;
}

void PopulateCustomItems(const WebVector<WebMenuItemInfo>& customItems,
    const std::string& prefix, std::vector<std::string>* strings) {
  for (size_t i = 0; i < customItems.size(); ++i) {
    if (customItems[i].type == blink::WebMenuItemInfo::Separator) {
      strings->push_back(prefix + kSeparatorIdentifier);
    } else if (customItems[i].type == blink::WebMenuItemInfo::SubMenu) {
      strings->push_back(prefix + customItems[i].label.utf8() +
          kSubMenuIdentifier);
      PopulateCustomItems(customItems[i].subMenuItems, prefix +
          kSubMenuDepthIdentifier, strings);
    } else {
      strings->push_back(prefix + customItems[i].label.utf8());
    }
  }
}

// Because actual context menu is implemented by the browser side,
// this function does only what LayoutTests are expecting:
// - Many test checks the count of items. So returning non-zero value makes
// sense.
// - Some test compares the count before and after some action. So changing the
// count based on flags also makes sense. This function is doing such for some
// flags.
// - Some test even checks actual string content. So providing it would be also
// helpful.
std::vector<std::string> MakeMenuItemStringsFor(
    WebContextMenuData* context_menu,
    WebTestDelegate* delegate) {
  // These constants are based on Safari's context menu because tests are made
  // for it.
  static const char* kNonEditableMenuStrings[] = {
    "Back",
    "Reload Page",
    "Open in Dashbaord",
    "<separator>",
    "View Source",
    "Save Page As",
    "Print Page",
    "Inspect Element",
    0
  };
  static const char* kEditableMenuStrings[] = {
    "Cut",
    "Copy",
    "<separator>",
    "Paste",
    "Spelling and Grammar",
    "Substitutions, Transformations",
    "Font",
    "Speech",
    "Paragraph Direction",
    "<separator>",
    0
  };

  // This is possible because mouse events are cancelleable.
  if (!context_menu)
    return std::vector<std::string>();

  std::vector<std::string> strings;

  // Populate custom menu items if provided by blink.
  PopulateCustomItems(context_menu->customItems, "", &strings);

  if (context_menu->isEditable) {
    for (const char** item = kEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
    WebVector<WebString> suggestions;
    MockSpellCheck::FillSuggestionList(context_menu->misspelledWord,
                                       &suggestions);
    for (size_t i = 0; i < suggestions.size(); ++i) {
      strings.push_back(suggestions[i].utf8());
    }
  } else {
    for (const char** item = kNonEditableMenuStrings; *item; ++item) {
      strings.push_back(*item);
    }
  }

  return strings;
}

// How much we should scroll per event - the value here is chosen to match the
// WebKit impl and layout test results.
const float kScrollbarPixelsPerTick = 40.0f;

WebMouseEvent::Button GetButtonTypeFromButtonNumber(int button_code) {
  if (!button_code)
    return WebMouseEvent::ButtonLeft;
  if (button_code == 2)
    return WebMouseEvent::ButtonRight;
  return WebMouseEvent::ButtonMiddle;
}

class MouseDownTask : public WebMethodTask<EventSender> {
 public:
  MouseDownTask(EventSender* obj, int button_number, int modifiers)
      : WebMethodTask<EventSender>(obj),
        button_number_(button_number),
        modifiers_(modifiers) {}

  virtual void RunIfValid() OVERRIDE {
    object_->MouseDown(button_number_, modifiers_);
  }

 private:
  int button_number_;
  int modifiers_;
};

class MouseUpTask : public WebMethodTask<EventSender> {
 public:
  MouseUpTask(EventSender* obj, int button_number, int modifiers)
      : WebMethodTask<EventSender>(obj),
        button_number_(button_number),
        modifiers_(modifiers) {}

  virtual void RunIfValid() OVERRIDE {
    object_->MouseUp(button_number_, modifiers_);
  }

 private:
  int button_number_;
  int modifiers_;
};

class KeyDownTask : public WebMethodTask<EventSender> {
 public:
  KeyDownTask(EventSender* obj,
              const std::string code_str,
              int modifiers,
              KeyLocationCode location)
      : WebMethodTask<EventSender>(obj),
        code_str_(code_str),
        modifiers_(modifiers),
        location_(location) {}

  virtual void RunIfValid() OVERRIDE {
    object_->KeyDown(code_str_, modifiers_, location_);
  }

 private:
  std::string code_str_;
  int modifiers_;
  KeyLocationCode location_;
};

bool NeedsShiftModifier(int keyCode) {
  // If code is an uppercase letter, assign a SHIFT key to eventDown.modifier.
  return (keyCode & 0xFF) >= 'A' && (keyCode & 0xFF) <= 'Z';
}

// Get the edit command corresponding to a keyboard event.
// Returns true if the specified event corresponds to an edit command, the name
// of the edit command will be stored in |*name|.
bool GetEditCommand(const WebKeyboardEvent& event, std::string* name) {
#if defined(OS_MACOSX)
// We only cares about Left,Right,Up,Down keys with Command or Command+Shift
// modifiers. These key events correspond to some special movement and
// selection editor commands. These keys will be marked as system key, which
// prevents them from being handled. Thus they must be handled specially.
  if ((event.modifiers & ~WebKeyboardEvent::ShiftKey) !=
      WebKeyboardEvent::MetaKey)
    return false;

  switch (event.windowsKeyCode) {
    case ui::VKEY_LEFT:
      *name = "MoveToBeginningOfLine";
      break;
    case ui::VKEY_RIGHT:
      *name = "MoveToEndOfLine";
      break;
    case ui::VKEY_UP:
      *name = "MoveToBeginningOfDocument";
      break;
    case ui::VKEY_DOWN:
      *name = "MoveToEndOfDocument";
      break;
    default:
      return false;
  }

  if (event.modifiers & WebKeyboardEvent::ShiftKey)
    name->append("AndModifySelection");

  return true;
#else
  return false;
#endif
}

bool IsSystemKeyEvent(const WebKeyboardEvent& event) {
#if defined(OS_MACOSX)
  return event.modifiers & WebInputEvent::MetaKey &&
      event.windowsKeyCode != ui::VKEY_B &&
      event.windowsKeyCode != ui::VKEY_I;
#else
  return !!(event.modifiers & WebInputEvent::AltKey);
#endif
}

}  // namespace

class EventSenderBindings : public gin::Wrappable<EventSenderBindings> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  static void Install(base::WeakPtr<EventSender> sender,
                      blink::WebFrame* frame);

 private:
  explicit EventSenderBindings(base::WeakPtr<EventSender> sender);
  virtual ~EventSenderBindings();

  // gin::Wrappable:
  virtual gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) OVERRIDE;

  // Bound methods:
  void EnableDOMUIEventLogging();
  void FireKeyboardEventsToElement();
  void ClearKillRing();
  std::vector<std::string> ContextClick();
  void TextZoomIn();
  void TextZoomOut();
  void ZoomPageIn();
  void ZoomPageOut();
  void SetPageZoomFactor(double factor);
  void SetPageScaleFactor(gin::Arguments* args);
  void ClearTouchPoints();
  void ReleaseTouchPoint(unsigned index);
  void UpdateTouchPoint(unsigned index, double x, double y);
  void CancelTouchPoint(unsigned index);
  void SetTouchModifier(const std::string& key_name, bool set_mask);
  void SetTouchCancelable(bool cancelable);
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
  // TODO(erikchen): Remove MouseMomentumBegin once CL 282743002 has landed.
  void MouseMomentumBegin();
  void MouseMomentumBegin2(gin::Arguments* args);
  void MouseMomentumScrollBy(gin::Arguments* args);
  void MouseMomentumEnd();
  void ScheduleAsynchronousClick(gin::Arguments* args);
  void ScheduleAsynchronousKeyDown(gin::Arguments* args);
  void MouseDown(gin::Arguments* args);
  void MouseUp(gin::Arguments* args);
  void KeyDown(gin::Arguments* args);

  // Binding properties:
  bool ForceLayoutOnEvents() const;
  void SetForceLayoutOnEvents(bool force);
  bool IsDragMode() const;
  void SetIsDragMode(bool drag_mode);

#if defined(OS_WIN)
  int WmKeyDown() const;
  void SetWmKeyDown(int key_down);

  int WmKeyUp() const;
  void SetWmKeyUp(int key_up);

  int WmChar() const;
  void SetWmChar(int wm_char);

  int WmDeadChar() const;
  void SetWmDeadChar(int dead_char);

  int WmSysKeyDown() const;
  void SetWmSysKeyDown(int key_down);

  int WmSysKeyUp() const;
  void SetWmSysKeyUp(int key_up);

  int WmSysChar() const;
  void SetWmSysChar(int sys_char);

  int WmSysDeadChar() const;
  void SetWmSysDeadChar(int sys_dead_char);
#endif

  base::WeakPtr<EventSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(EventSenderBindings);
};

gin::WrapperInfo EventSenderBindings::kWrapperInfo = {gin::kEmbedderNativeGin};

EventSenderBindings::EventSenderBindings(base::WeakPtr<EventSender> sender)
    : sender_(sender) {
}

EventSenderBindings::~EventSenderBindings() {}

// static
void EventSenderBindings::Install(base::WeakPtr<EventSender> sender,
                                  WebFrame* frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Context> context = frame->mainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Context::Scope context_scope(context);

  gin::Handle<EventSenderBindings> bindings =
      gin::CreateHandle(isolate, new EventSenderBindings(sender));
  if (bindings.IsEmpty())
    return;
  v8::Handle<v8::Object> global = context->Global();
  global->Set(gin::StringToV8(isolate, "eventSender"), bindings.ToV8());
}

gin::ObjectTemplateBuilder
EventSenderBindings::GetObjectTemplateBuilder(v8::Isolate* isolate) {
  return gin::Wrappable<EventSenderBindings>::GetObjectTemplateBuilder(isolate)
      .SetMethod("enableDOMUIEventLogging",
                 &EventSenderBindings::EnableDOMUIEventLogging)
      .SetMethod("fireKeyboardEventsToElement",
                 &EventSenderBindings::FireKeyboardEventsToElement)
      .SetMethod("clearKillRing", &EventSenderBindings::ClearKillRing)
      .SetMethod("contextClick", &EventSenderBindings::ContextClick)
      .SetMethod("textZoomIn", &EventSenderBindings::TextZoomIn)
      .SetMethod("textZoomOut", &EventSenderBindings::TextZoomOut)
      .SetMethod("zoomPageIn", &EventSenderBindings::ZoomPageIn)
      .SetMethod("zoomPageOut", &EventSenderBindings::ZoomPageOut)
      .SetMethod("setPageZoomFactor", &EventSenderBindings::SetPageZoomFactor)
      .SetMethod("setPageScaleFactor", &EventSenderBindings::SetPageScaleFactor)
      .SetMethod("clearTouchPoints", &EventSenderBindings::ClearTouchPoints)
      .SetMethod("releaseTouchPoint", &EventSenderBindings::ReleaseTouchPoint)
      .SetMethod("updateTouchPoint", &EventSenderBindings::UpdateTouchPoint)
      .SetMethod("cancelTouchPoint", &EventSenderBindings::CancelTouchPoint)
      .SetMethod("setTouchModifier", &EventSenderBindings::SetTouchModifier)
      .SetMethod("setTouchCancelable", &EventSenderBindings::SetTouchCancelable)
      .SetMethod("dumpFilenameBeingDragged",
                 &EventSenderBindings::DumpFilenameBeingDragged)
      .SetMethod("gestureFlingCancel", &EventSenderBindings::GestureFlingCancel)
      .SetMethod("gestureFlingStart", &EventSenderBindings::GestureFlingStart)
      .SetMethod("gestureScrollFirstPoint",
                 &EventSenderBindings::GestureScrollFirstPoint)
      .SetMethod("touchStart", &EventSenderBindings::TouchStart)
      .SetMethod("touchMove", &EventSenderBindings::TouchMove)
      .SetMethod("touchCancel", &EventSenderBindings::TouchCancel)
      .SetMethod("touchEnd", &EventSenderBindings::TouchEnd)
      .SetMethod("leapForward", &EventSenderBindings::LeapForward)
      .SetMethod("beginDragWithFiles", &EventSenderBindings::BeginDragWithFiles)
      .SetMethod("addTouchPoint", &EventSenderBindings::AddTouchPoint)
      .SetMethod("mouseDragBegin", &EventSenderBindings::MouseDragBegin)
      .SetMethod("mouseDragEnd", &EventSenderBindings::MouseDragEnd)
      .SetMethod("gestureScrollBegin", &EventSenderBindings::GestureScrollBegin)
      .SetMethod("gestureScrollEnd", &EventSenderBindings::GestureScrollEnd)
      .SetMethod("gestureScrollUpdate",
                 &EventSenderBindings::GestureScrollUpdate)
      .SetMethod("gestureScrollUpdateWithoutPropagation",
                 &EventSenderBindings::GestureScrollUpdateWithoutPropagation)
      .SetMethod("gestureTap", &EventSenderBindings::GestureTap)
      .SetMethod("gestureTapDown", &EventSenderBindings::GestureTapDown)
      .SetMethod("gestureShowPress", &EventSenderBindings::GestureShowPress)
      .SetMethod("gestureTapCancel", &EventSenderBindings::GestureTapCancel)
      .SetMethod("gestureLongPress", &EventSenderBindings::GestureLongPress)
      .SetMethod("gestureLongTap", &EventSenderBindings::GestureLongTap)
      .SetMethod("gestureTwoFingerTap",
                 &EventSenderBindings::GestureTwoFingerTap)
      .SetMethod("continuousMouseScrollBy",
                 &EventSenderBindings::ContinuousMouseScrollBy)
      .SetMethod("keyDown", &EventSenderBindings::KeyDown)
      .SetMethod("mouseDown", &EventSenderBindings::MouseDown)
      .SetMethod("mouseMoveTo", &EventSenderBindings::MouseMoveTo)
      .SetMethod("trackpadScrollBegin",
                 &EventSenderBindings::TrackpadScrollBegin)
      .SetMethod("trackpadScroll", &EventSenderBindings::TrackpadScroll)
      .SetMethod("trackpadScrollEnd", &EventSenderBindings::TrackpadScrollEnd)
      .SetMethod("mouseScrollBy", &EventSenderBindings::MouseScrollBy)
      .SetMethod("mouseUp", &EventSenderBindings::MouseUp)
      .SetMethod("mouseMomentumBegin", &EventSenderBindings::MouseMomentumBegin)
      .SetMethod("mouseMomentumBegin2",
                 &EventSenderBindings::MouseMomentumBegin2)
      .SetMethod("mouseMomentumScrollBy",
                 &EventSenderBindings::MouseMomentumScrollBy)
      .SetMethod("mouseMomentumEnd", &EventSenderBindings::MouseMomentumEnd)
      .SetMethod("scheduleAsynchronousClick",
                 &EventSenderBindings::ScheduleAsynchronousClick)
      .SetMethod("scheduleAsynchronousKeyDown",
                 &EventSenderBindings::ScheduleAsynchronousKeyDown)
      .SetProperty("forceLayoutOnEvents",
                   &EventSenderBindings::ForceLayoutOnEvents,
                   &EventSenderBindings::SetForceLayoutOnEvents)
      .SetProperty("dragMode",
                   &EventSenderBindings::IsDragMode,
                   &EventSenderBindings::SetIsDragMode)
#if defined(OS_WIN)
      .SetProperty("WM_KEYDOWN",
                   &EventSenderBindings::WmKeyDown,
                   &EventSenderBindings::SetWmKeyDown)
      .SetProperty("WM_KEYUP",
                   &EventSenderBindings::WmKeyUp,
                   &EventSenderBindings::SetWmKeyUp)
      .SetProperty("WM_CHAR",
                   &EventSenderBindings::WmChar,
                   &EventSenderBindings::SetWmChar)
      .SetProperty("WM_DEADCHAR",
                   &EventSenderBindings::WmDeadChar,
                   &EventSenderBindings::SetWmDeadChar)
      .SetProperty("WM_SYSKEYDOWN",
                   &EventSenderBindings::WmSysKeyDown,
                   &EventSenderBindings::SetWmSysKeyDown)
      .SetProperty("WM_SYSKEYUP",
                   &EventSenderBindings::WmSysKeyUp,
                   &EventSenderBindings::SetWmSysKeyUp)
      .SetProperty("WM_SYSCHAR",
                   &EventSenderBindings::WmSysChar,
                   &EventSenderBindings::SetWmSysChar)
      .SetProperty("WM_SYSDEADCHAR",
                   &EventSenderBindings::WmSysDeadChar,
                   &EventSenderBindings::SetWmSysDeadChar);
#else
      ;
#endif
}

void EventSenderBindings::EnableDOMUIEventLogging() {
  if (sender_)
    sender_->EnableDOMUIEventLogging();
}

void EventSenderBindings::FireKeyboardEventsToElement() {
  if (sender_)
    sender_->FireKeyboardEventsToElement();
}

void EventSenderBindings::ClearKillRing() {
  if (sender_)
    sender_->ClearKillRing();
}

std::vector<std::string> EventSenderBindings::ContextClick() {
  if (sender_)
    return sender_->ContextClick();
  return std::vector<std::string>();
}

void EventSenderBindings::TextZoomIn() {
  if (sender_)
    sender_->TextZoomIn();
}

void EventSenderBindings::TextZoomOut() {
  if (sender_)
    sender_->TextZoomOut();
}

void EventSenderBindings::ZoomPageIn() {
  if (sender_)
    sender_->ZoomPageIn();
}

void EventSenderBindings::ZoomPageOut() {
  if (sender_)
    sender_->ZoomPageOut();
}

void EventSenderBindings::SetPageZoomFactor(double factor) {
  if (sender_)
    sender_->SetPageZoomFactor(factor);
}

void EventSenderBindings::SetPageScaleFactor(gin::Arguments* args) {
  if (!sender_)
    return;
  float scale_factor;
  double x;
  double y;
  if (args->PeekNext().IsEmpty())
    return;
  args->GetNext(&scale_factor);
  if (args->PeekNext().IsEmpty())
    return;
  args->GetNext(&x);
  if (args->PeekNext().IsEmpty())
    return;
  args->GetNext(&y);
  sender_->SetPageScaleFactor(scale_factor,
                              static_cast<int>(x), static_cast<int>(y));
}

void EventSenderBindings::ClearTouchPoints() {
  if (sender_)
    sender_->ClearTouchPoints();
}

void EventSenderBindings::ReleaseTouchPoint(unsigned index) {
  if (sender_)
    sender_->ReleaseTouchPoint(index);
}

void EventSenderBindings::UpdateTouchPoint(unsigned index, double x, double y) {
  if (sender_)
    sender_->UpdateTouchPoint(index, static_cast<float>(x), static_cast<float>(y));
}

void EventSenderBindings::CancelTouchPoint(unsigned index) {
  if (sender_)
    sender_->CancelTouchPoint(index);
}

void EventSenderBindings::SetTouchModifier(const std::string& key_name,
                                           bool set_mask) {
  if (sender_)
    sender_->SetTouchModifier(key_name, set_mask);
}

void EventSenderBindings::SetTouchCancelable(bool cancelable) {
  if (sender_)
    sender_->SetTouchCancelable(cancelable);
}

void EventSenderBindings::DumpFilenameBeingDragged() {
  if (sender_)
    sender_->DumpFilenameBeingDragged();
}

void EventSenderBindings::GestureFlingCancel() {
  if (sender_)
    sender_->GestureFlingCancel();
}

void EventSenderBindings::GestureFlingStart(float x,
                                            float y,
                                            float velocity_x,
                                            float velocity_y) {
  if (sender_)
    sender_->GestureFlingStart(x, y, velocity_x, velocity_y);
}

void EventSenderBindings::GestureScrollFirstPoint(int x, int y) {
  if (sender_)
    sender_->GestureScrollFirstPoint(x, y);
}

void EventSenderBindings::TouchStart() {
  if (sender_)
    sender_->TouchStart();
}

void EventSenderBindings::TouchMove() {
  if (sender_)
    sender_->TouchMove();
}

void EventSenderBindings::TouchCancel() {
  if (sender_)
    sender_->TouchCancel();
}

void EventSenderBindings::TouchEnd() {
  if (sender_)
    sender_->TouchEnd();
}

void EventSenderBindings::LeapForward(int milliseconds) {
  if (sender_)
    sender_->LeapForward(milliseconds);
}

void EventSenderBindings::BeginDragWithFiles(
    const std::vector<std::string>& files) {
  if (sender_)
    sender_->BeginDragWithFiles(files);
}

void EventSenderBindings::AddTouchPoint(gin::Arguments* args) {
  if (sender_)
    sender_->AddTouchPoint(args);
}

void EventSenderBindings::MouseDragBegin() {
  if (sender_)
    sender_->MouseDragBegin();
}

void EventSenderBindings::MouseDragEnd() {
  if (sender_)
    sender_->MouseDragEnd();
}

void EventSenderBindings::GestureScrollBegin(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollBegin(args);
}

void EventSenderBindings::GestureScrollEnd(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollEnd(args);
}

void EventSenderBindings::GestureScrollUpdate(gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollUpdate(args);
}

void EventSenderBindings::GestureScrollUpdateWithoutPropagation(
    gin::Arguments* args) {
  if (sender_)
    sender_->GestureScrollUpdateWithoutPropagation(args);
}

void EventSenderBindings::GestureTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTap(args);
}

void EventSenderBindings::GestureTapDown(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapDown(args);
}

void EventSenderBindings::GestureShowPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureShowPress(args);
}

void EventSenderBindings::GestureTapCancel(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTapCancel(args);
}

void EventSenderBindings::GestureLongPress(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongPress(args);
}

void EventSenderBindings::GestureLongTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureLongTap(args);
}

void EventSenderBindings::GestureTwoFingerTap(gin::Arguments* args) {
  if (sender_)
    sender_->GestureTwoFingerTap(args);
}

void EventSenderBindings::ContinuousMouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->ContinuousMouseScrollBy(args);
}

void EventSenderBindings::MouseMoveTo(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMoveTo(args);
}

void EventSenderBindings::TrackpadScrollBegin() {
  if (sender_)
    sender_->TrackpadScrollBegin();
}

void EventSenderBindings::TrackpadScroll(gin::Arguments* args) {
  if (sender_)
    sender_->TrackpadScroll(args);
}

void EventSenderBindings::TrackpadScrollEnd() {
  if (sender_)
    sender_->TrackpadScrollEnd();
}

void EventSenderBindings::MouseScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseScrollBy(args);
}

void EventSenderBindings::MouseMomentumBegin() {
  if (sender_)
    sender_->MouseMomentumBegin();
}

void EventSenderBindings::MouseMomentumBegin2(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMomentumBegin2(args);
}

void EventSenderBindings::MouseMomentumScrollBy(gin::Arguments* args) {
  if (sender_)
    sender_->MouseMomentumScrollBy(args);
}

void EventSenderBindings::MouseMomentumEnd() {
  if (sender_)
    sender_->MouseMomentumEnd();
}

void EventSenderBindings::ScheduleAsynchronousClick(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&button_number);
    if (!args->PeekNext().IsEmpty())
      modifiers = GetKeyModifiersFromV8(args->PeekNext());
  }
  sender_->ScheduleAsynchronousClick(button_number, modifiers);
}

void EventSenderBindings::ScheduleAsynchronousKeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Handle<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->ScheduleAsynchronousKeyDown(code_str, modifiers,
                                       static_cast<KeyLocationCode>(location));
}

void EventSenderBindings::MouseDown(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&button_number);
    if (!args->PeekNext().IsEmpty())
      modifiers = GetKeyModifiersFromV8(args->PeekNext());
  }
  sender_->MouseDown(button_number, modifiers);
}

void EventSenderBindings::MouseUp(gin::Arguments* args) {
  if (!sender_)
    return;

  int button_number = 0;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&button_number);
    if (!args->PeekNext().IsEmpty())
      modifiers = GetKeyModifiersFromV8(args->PeekNext());
  }
  sender_->MouseUp(button_number, modifiers);
}

void EventSenderBindings::KeyDown(gin::Arguments* args) {
  if (!sender_)
    return;

  std::string code_str;
  int modifiers = 0;
  int location = DOMKeyLocationStandard;
  args->GetNext(&code_str);
  if (!args->PeekNext().IsEmpty()) {
    v8::Handle<v8::Value> value;
    args->GetNext(&value);
    modifiers = GetKeyModifiersFromV8(value);
    if (!args->PeekNext().IsEmpty())
      args->GetNext(&location);
  }
  sender_->KeyDown(code_str, modifiers, static_cast<KeyLocationCode>(location));
}

bool EventSenderBindings::ForceLayoutOnEvents() const {
  if (sender_)
    return sender_->force_layout_on_events();
  return false;
}

void EventSenderBindings::SetForceLayoutOnEvents(bool force) {
  if (sender_)
    sender_->set_force_layout_on_events(force);
}

bool EventSenderBindings::IsDragMode() const {
  if (sender_)
    return sender_->is_drag_mode();
  return true;
}

void EventSenderBindings::SetIsDragMode(bool drag_mode) {
  if (sender_)
    sender_->set_is_drag_mode(drag_mode);
}

#if defined(OS_WIN)
int EventSenderBindings::WmKeyDown() const {
  if (sender_)
    return sender_->wm_key_down();
  return 0;
}

void EventSenderBindings::SetWmKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_key_down(key_down);
}

int EventSenderBindings::WmKeyUp() const {
  if (sender_)
    return sender_->wm_key_up();
  return 0;
}

void EventSenderBindings::SetWmKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_key_up(key_up);
}

int EventSenderBindings::WmChar() const {
  if (sender_)
    return sender_->wm_char();
  return 0;
}

void EventSenderBindings::SetWmChar(int wm_char) {
  if (sender_)
    sender_->set_wm_char(wm_char);
}

int EventSenderBindings::WmDeadChar() const {
  if (sender_)
    return sender_->wm_dead_char();
  return 0;
}

void EventSenderBindings::SetWmDeadChar(int dead_char) {
  if (sender_)
    sender_->set_wm_dead_char(dead_char);
}

int EventSenderBindings::WmSysKeyDown() const {
  if (sender_)
    return sender_->wm_sys_key_down();
  return 0;
}

void EventSenderBindings::SetWmSysKeyDown(int key_down) {
  if (sender_)
    sender_->set_wm_sys_key_down(key_down);
}

int EventSenderBindings::WmSysKeyUp() const {
  if (sender_)
    return sender_->wm_sys_key_up();
  return 0;
}

void EventSenderBindings::SetWmSysKeyUp(int key_up) {
  if (sender_)
    sender_->set_wm_sys_key_up(key_up);
}

int EventSenderBindings::WmSysChar() const {
  if (sender_)
    return sender_->wm_sys_char();
  return 0;
}

void EventSenderBindings::SetWmSysChar(int sys_char) {
  if (sender_)
    sender_->set_wm_sys_char(sys_char);
}

int EventSenderBindings::WmSysDeadChar() const {
  if (sender_)
    return sender_->wm_sys_dead_char();
  return 0;
}

void EventSenderBindings::SetWmSysDeadChar(int sys_dead_char) {
  if (sender_)
    sender_->set_wm_sys_dead_char(sys_dead_char);
}
#endif

// EventSender -----------------------------------------------------------------

WebMouseEvent::Button EventSender::pressed_button_ = WebMouseEvent::ButtonNone;

WebPoint EventSender::last_mouse_pos_;

WebMouseEvent::Button EventSender::last_button_type_ =
    WebMouseEvent::ButtonNone;

EventSender::SavedEvent::SavedEvent()
    : type(TYPE_UNSPECIFIED),
      button_type(WebMouseEvent::ButtonNone),
      milliseconds(0),
      modifiers(0) {}

EventSender::EventSender(TestInterfaces* interfaces)
    : interfaces_(interfaces),
      delegate_(NULL),
      view_(NULL),
      force_layout_on_events_(false),
      is_drag_mode_(true),
      touch_modifiers_(0),
      touch_cancelable_(true),
      replaying_saved_events_(false),
      current_drag_effects_allowed_(blink::WebDragOperationNone),
      last_click_time_sec_(0),
      current_drag_effect_(blink::WebDragOperationNone),
      time_offset_ms_(0),
      click_count_(0),
#if defined(OS_WIN)
      wm_key_down_(0),
      wm_key_up_(0),
      wm_char_(0),
      wm_dead_char_(0),
      wm_sys_key_down_(0),
      wm_sys_key_up_(0),
      wm_sys_char_(0),
      wm_sys_dead_char_(0),
#endif
      weak_factory_(this) {}

EventSender::~EventSender() {}

void EventSender::Reset() {
  DCHECK(current_drag_data_.isNull());
  current_drag_data_.reset();
  current_drag_effect_ = blink::WebDragOperationNone;
  current_drag_effects_allowed_ = blink::WebDragOperationNone;
  if (view_ && pressed_button_ != WebMouseEvent::ButtonNone)
    view_->mouseCaptureLost();
  pressed_button_ = WebMouseEvent::ButtonNone;
  is_drag_mode_ = true;
  force_layout_on_events_ = true;

#if defined(OS_WIN)
  wm_key_down_ = WM_KEYDOWN;
  wm_key_up_ = WM_KEYUP;
  wm_char_ = WM_CHAR;
  wm_dead_char_ = WM_DEADCHAR;
  wm_sys_key_down_ = WM_SYSKEYDOWN;
  wm_sys_key_up_ = WM_SYSKEYUP;
  wm_sys_char_ = WM_SYSCHAR;
  wm_sys_dead_char_ = WM_SYSDEADCHAR;
#endif

  last_mouse_pos_ = WebPoint(0, 0);
  last_click_time_sec_ = 0;
  last_click_pos_ = WebPoint(0, 0);
  last_button_type_ = WebMouseEvent::ButtonNone;
  touch_points_.clear();
  last_context_menu_data_.reset();
  task_list_.RevokeAll();
  current_gesture_location_ = WebPoint(0, 0);
  mouse_event_queue_.clear();

  time_offset_ms_ = 0;
  click_count_ = 0;

  touch_modifiers_ = 0;
  touch_cancelable_ = true;
  touch_points_.clear();
}

void EventSender::Install(WebFrame* frame) {
  EventSenderBindings::Install(weak_factory_.GetWeakPtr(), frame);
}

void EventSender::SetDelegate(WebTestDelegate* delegate) {
  delegate_ = delegate;
}

void EventSender::SetWebView(WebView* view) {
  view_ = view;
}

void EventSender::SetContextMenuData(const WebContextMenuData& data) {
  last_context_menu_data_.reset(new WebContextMenuData(data));
}

void EventSender::DoDragDrop(const WebDragData& drag_data,
                              WebDragOperationsMask mask) {
  WebMouseEvent event;
  InitMouseEvent(WebInputEvent::MouseDown,
                 pressed_button_,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  WebPoint client_point(event.x, event.y);
  WebPoint screen_point(event.globalX, event.globalY);
  current_drag_data_ = drag_data;
  current_drag_effects_allowed_ = mask;
  current_drag_effect_ = view_->dragTargetDragEnter(
      drag_data, client_point, screen_point, current_drag_effects_allowed_, 0);

  // Finish processing events.
  ReplaySavedEvents();
}

void EventSender::MouseDown(int button_number, int modifiers) {
  if (force_layout_on_events_)
    view_->layout();

  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  UpdateClickCountForButton(button_type);

  pressed_button_ = button_type;

  WebMouseEvent event;
  InitMouseEvent(WebInputEvent::MouseDown,
                 button_type,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 modifiers,
                 &event);
  view_->handleInputEvent(event);
}

void EventSender::MouseUp(int button_number, int modifiers) {
  if (force_layout_on_events_)
    view_->layout();

  DCHECK_NE(-1, button_number);

  WebMouseEvent::Button button_type =
      GetButtonTypeFromButtonNumber(button_number);

  if (is_drag_mode_ && !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_UP;
    saved_event.button_type = button_type;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
    ReplaySavedEvents();
  } else {
    WebMouseEvent event;
    InitMouseEvent(WebInputEvent::MouseUp,
                   button_type,
                   last_mouse_pos_,
                   GetCurrentEventTimeSec(),
                   click_count_,
                   modifiers,
                   &event);
    DoMouseUp(event);
  }
}

void EventSender::KeyDown(const std::string& code_str,
                          int modifiers,
                          KeyLocationCode location) {
  // FIXME: I'm not exactly sure how we should convert the string to a key
  // event. This seems to work in the cases I tested.
  // FIXME: Should we also generate a KEY_UP?

  bool generate_char = false;

  // Convert \n -> VK_RETURN. Some layout tests use \n to mean "Enter", when
  // Windows uses \r for "Enter".
  int code = 0;
  int text = 0;
  bool needs_shift_key_modifier = false;

  if ("\n" == code_str) {
    generate_char = true;
    text = code = ui::VKEY_RETURN;
  } else if ("rightArrow" == code_str) {
    code = ui::VKEY_RIGHT;
  } else if ("downArrow" == code_str) {
    code = ui::VKEY_DOWN;
  } else if ("leftArrow" == code_str) {
    code = ui::VKEY_LEFT;
  } else if ("upArrow" == code_str) {
    code = ui::VKEY_UP;
  } else if ("insert" == code_str) {
    code = ui::VKEY_INSERT;
  } else if ("delete" == code_str) {
    code = ui::VKEY_DELETE;
  } else if ("pageUp" == code_str) {
    code = ui::VKEY_PRIOR;
  } else if ("pageDown" == code_str) {
    code = ui::VKEY_NEXT;
  } else if ("home" == code_str) {
    code = ui::VKEY_HOME;
  } else if ("end" == code_str) {
    code = ui::VKEY_END;
  } else if ("printScreen" == code_str) {
    code = ui::VKEY_SNAPSHOT;
  } else if ("menu" == code_str) {
    code = ui::VKEY_APPS;
  } else if ("leftControl" == code_str) {
    code = ui::VKEY_LCONTROL;
  } else if ("rightControl" == code_str) {
    code = ui::VKEY_RCONTROL;
  } else if ("leftShift" == code_str) {
    code = ui::VKEY_LSHIFT;
  } else if ("rightShift" == code_str) {
    code = ui::VKEY_RSHIFT;
  } else if ("leftAlt" == code_str) {
    code = ui::VKEY_LMENU;
  } else if ("rightAlt" == code_str) {
    code = ui::VKEY_RMENU;
  } else if ("numLock" == code_str) {
    code = ui::VKEY_NUMLOCK;
  } else if ("backspace" == code_str) {
    code = ui::VKEY_BACK;
  } else if ("escape" == code_str) {
    code = ui::VKEY_ESCAPE;
  } else {
    // Compare the input string with the function-key names defined by the
    // DOM spec (i.e. "F1",...,"F24"). If the input string is a function-key
    // name, set its key code.
    for (int i = 1; i <= 24; ++i) {
      std::string function_key_name = base::StringPrintf("F%d", i);
      if (function_key_name == code_str) {
        code = ui::VKEY_F1 + (i - 1);
        break;
      }
    }
    if (!code) {
      WebString web_code_str =
          WebString::fromUTF8(code_str.data(), code_str.size());
      DCHECK_EQ(1u, web_code_str.length());
      text = code = web_code_str.at(0);
      needs_shift_key_modifier = NeedsShiftModifier(code);
      if ((code & 0xFF) >= 'a' && (code & 0xFF) <= 'z')
        code -= 'a' - 'A';
      generate_char = true;
    }

    if ("(" == code_str) {
      code = '9';
      needs_shift_key_modifier = true;
    }
  }

  // For one generated keyboard event, we need to generate a keyDown/keyUp
  // pair;
  // On Windows, we might also need to generate a char event to mimic the
  // Windows event flow; on other platforms we create a merged event and test
  // the event flow that that platform provides.
  WebKeyboardEvent event_down;
  event_down.type = WebInputEvent::RawKeyDown;
  event_down.modifiers = modifiers;
  event_down.windowsKeyCode = code;

  if (generate_char) {
    event_down.text[0] = text;
    event_down.unmodifiedText[0] = text;
  }

  event_down.setKeyIdentifierFromWindowsKeyCode();

  if (event_down.modifiers != 0)
    event_down.isSystemKey = IsSystemKeyEvent(event_down);

  if (needs_shift_key_modifier)
    event_down.modifiers |= WebInputEvent::ShiftKey;

  // See if KeyLocation argument is given.
  if (location == DOMKeyLocationNumpad)
    event_down.modifiers |= WebInputEvent::IsKeyPad;

  WebKeyboardEvent event_up;
  event_up = event_down;
  event_up.type = WebInputEvent::KeyUp;
  // EventSender.m forces a layout here, with at least one
  // test (fast/forms/focus-control-to-page.html) relying on this.
  if (force_layout_on_events_)
    view_->layout();

  // In the browser, if a keyboard event corresponds to an editor command,
  // the command will be dispatched to the renderer just before dispatching
  // the keyboard event, and then it will be executed in the
  // RenderView::handleCurrentKeyboardEvent() method.
  // We just simulate the same behavior here.
  std::string edit_command;
  if (GetEditCommand(event_down, &edit_command))
    delegate_->SetEditCommand(edit_command, "");

  view_->handleInputEvent(event_down);

  if (code == ui::VKEY_ESCAPE && !current_drag_data_.isNull()) {
    WebMouseEvent event;
    InitMouseEvent(WebInputEvent::MouseDown,
                   pressed_button_,
                   last_mouse_pos_,
                   GetCurrentEventTimeSec(),
                   click_count_,
                   0,
                   &event);
    FinishDragAndDrop(event, blink::WebDragOperationNone);
  }

  delegate_->ClearEditCommand();

  if (generate_char) {
    WebKeyboardEvent event_char = event_up;
    event_char.type = WebInputEvent::Char;
    event_char.keyIdentifier[0] = '\0';
    view_->handleInputEvent(event_char);
  }

  view_->handleInputEvent(event_up);
}

void EventSender::EnableDOMUIEventLogging() {}

void EventSender::FireKeyboardEventsToElement() {}

void EventSender::ClearKillRing() {}

std::vector<std::string> EventSender::ContextClick() {
  if (force_layout_on_events_) {
    view_->layout();
  }

  UpdateClickCountForButton(WebMouseEvent::ButtonRight);

  // Clears last context menu data because we need to know if the context menu
  // be requested after following mouse events.
  last_context_menu_data_.reset();

  // Generate right mouse down and up.
  WebMouseEvent event;
  // This is a hack to work around only allowing a single pressed button since
  // we want to test the case where both the left and right mouse buttons are
  // pressed.
  if (pressed_button_ == WebMouseEvent::ButtonNone) {
    pressed_button_ = WebMouseEvent::ButtonRight;
  }
  InitMouseEvent(WebInputEvent::MouseDown,
                 WebMouseEvent::ButtonRight,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  view_->handleInputEvent(event);

#if defined(OS_WIN)
  InitMouseEvent(WebInputEvent::MouseUp,
                 WebMouseEvent::ButtonRight,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  view_->handleInputEvent(event);

  pressed_button_= WebMouseEvent::ButtonNone;
#endif

  std::vector<std::string> menu_items = MakeMenuItemStringsFor(last_context_menu_data_.get(), delegate_);
  last_context_menu_data_.reset();
  return menu_items;
}

void EventSender::TextZoomIn() {
  view_->setTextZoomFactor(view_->textZoomFactor() * 1.2f);
}

void EventSender::TextZoomOut() {
  view_->setTextZoomFactor(view_->textZoomFactor() / 1.2f);
}

void EventSender::ZoomPageIn() {
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces_->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->GetWebView()->setZoomLevel(
        window_list.at(i)->GetWebView()->zoomLevel() + 1);
  }
}

void EventSender::ZoomPageOut() {
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces_->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->GetWebView()->setZoomLevel(
        window_list.at(i)->GetWebView()->zoomLevel() - 1);
  }
}

void EventSender::SetPageZoomFactor(double zoom_factor) {
  const std::vector<WebTestProxyBase*>& window_list =
      interfaces_->GetWindowList();

  for (size_t i = 0; i < window_list.size(); ++i) {
    window_list.at(i)->GetWebView()->setZoomLevel(
        ZoomFactorToZoomLevel(zoom_factor));
  }
}

void EventSender::SetPageScaleFactor(float scale_factor, int x, int y) {
  view_->setPageScaleFactorLimits(scale_factor, scale_factor);
  view_->setPageScaleFactor(scale_factor, WebPoint(x, y));
}

void EventSender::ClearTouchPoints() {
  touch_points_.clear();
}

void EventSender::ThrowTouchPointError() {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  isolate->ThrowException(v8::Exception::TypeError(
      gin::StringToV8(isolate, "Invalid touch point.")));
}

void EventSender::ReleaseTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateReleased;
}

void EventSender::UpdateTouchPoint(unsigned index, float x, float y) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateMoved;
  touch_point->position = WebFloatPoint(x, y);
  touch_point->screenPosition = touch_point->position;
}

void EventSender::CancelTouchPoint(unsigned index) {
  if (index >= touch_points_.size()) {
    ThrowTouchPointError();
    return;
  }

  WebTouchPoint* touch_point = &touch_points_[index];
  touch_point->state = WebTouchPoint::StateCancelled;
}

void EventSender::SetTouchModifier(const std::string& key_name,
                                    bool set_mask) {
  int mask = 0;
  if (key_name == "shift")
    mask = WebInputEvent::ShiftKey;
  else if (key_name == "alt")
    mask = WebInputEvent::AltKey;
  else if (key_name == "ctrl")
    mask = WebInputEvent::ControlKey;
  else if (key_name == "meta")
    mask = WebInputEvent::MetaKey;

  if (set_mask)
    touch_modifiers_ |= mask;
  else
    touch_modifiers_ &= ~mask;
}

void EventSender::SetTouchCancelable(bool cancelable) {
  touch_cancelable_ = cancelable;
}

void EventSender::DumpFilenameBeingDragged() {
  WebString filename;
  WebVector<WebDragData::Item> items = current_drag_data_.items();
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].storageType == WebDragData::Item::StorageTypeBinaryData) {
      filename = items[i].title;
      break;
    }
  }
  delegate_->PrintMessage(std::string("Filename being dragged: ") +
                          filename.utf8().data() + "\n");
}

void EventSender::GestureFlingCancel() {
  WebGestureEvent event;
  event.type = WebInputEvent::GestureFlingCancel;
  event.timeStampSeconds = GetCurrentEventTimeSec();

  if (force_layout_on_events_)
    view_->layout();

  view_->handleInputEvent(event);
}

void EventSender::GestureFlingStart(float x,
                                     float y,
                                     float velocity_x,
                                     float velocity_y) {
  WebGestureEvent event;
  event.type = WebInputEvent::GestureFlingStart;

  event.x = x;
  event.y = y;
  event.globalX = event.x;
  event.globalY = event.y;

  event.data.flingStart.velocityX = velocity_x;
  event.data.flingStart.velocityY = velocity_y;
  event.timeStampSeconds = GetCurrentEventTimeSec();

  if (force_layout_on_events_)
    view_->layout();

  view_->handleInputEvent(event);
}

void EventSender::GestureScrollFirstPoint(int x, int y) {
  current_gesture_location_ = WebPoint(x, y);
}

void EventSender::TouchStart() {
  SendCurrentTouchEvent(WebInputEvent::TouchStart);
}

void EventSender::TouchMove() {
  SendCurrentTouchEvent(WebInputEvent::TouchMove);
}

void EventSender::TouchCancel() {
  SendCurrentTouchEvent(WebInputEvent::TouchCancel);
}

void EventSender::TouchEnd() {
  SendCurrentTouchEvent(WebInputEvent::TouchEnd);
}

void EventSender::LeapForward(int milliseconds) {
  if (is_drag_mode_ && pressed_button_ == WebMouseEvent::ButtonLeft &&
      !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_LEAP_FORWARD;
    saved_event.milliseconds = milliseconds;
    mouse_event_queue_.push_back(saved_event);
  } else {
    DoLeapForward(milliseconds);
  }
}

void EventSender::BeginDragWithFiles(const std::vector<std::string>& files) {
  current_drag_data_.initialize();
  WebVector<WebString> absolute_filenames(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    WebDragData::Item item;
    item.storageType = WebDragData::Item::StorageTypeFilename;
    item.filenameData = delegate_->GetAbsoluteWebStringFromUTF8Path(files[i]);
    current_drag_data_.addItem(item);
    absolute_filenames[i] = item.filenameData;
  }
  current_drag_data_.setFilesystemId(
      delegate_->RegisterIsolatedFileSystem(absolute_filenames));
  current_drag_effects_allowed_ = blink::WebDragOperationCopy;

  // Provide a drag source.
  view_->dragTargetDragEnter(current_drag_data_,
                             last_mouse_pos_,
                             last_mouse_pos_,
                             current_drag_effects_allowed_,
                             0);
  // |is_drag_mode_| saves events and then replays them later. We don't
  // need/want that.
  is_drag_mode_ = false;

  // Make the rest of eventSender think a drag is in progress.
  pressed_button_ = WebMouseEvent::ButtonLeft;
}

void EventSender::AddTouchPoint(gin::Arguments* args) {
  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }

  WebTouchPoint touch_point;
  touch_point.state = WebTouchPoint::StatePressed;
  touch_point.position = WebFloatPoint(static_cast<float>(x),
                                       static_cast<float>(y));
  touch_point.screenPosition = touch_point.position;

  if (!args->PeekNext().IsEmpty()) {
    double radius_x;
    if (!args->GetNext(&radius_x)) {
      args->ThrowError();
      return;
    }

    double radius_y = radius_x;
    if (!args->PeekNext().IsEmpty()) {
      if (!args->GetNext(&radius_y)) {
        args->ThrowError();
        return;
      }
    }

    touch_point.radiusX = static_cast<float>(radius_x);
    touch_point.radiusY = static_cast<float>(radius_y);
  }

  int lowest_id = 0;
  for (size_t i = 0; i < touch_points_.size(); i++) {
    if (touch_points_[i].id == lowest_id)
      lowest_id++;
  }
  touch_point.id = lowest_id;
  touch_points_.push_back(touch_point);
}

void EventSender::MouseDragBegin() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.phase = WebMouseWheelEvent::PhaseBegan;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::MouseDragEnd() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.phase = WebMouseWheelEvent::PhaseEnded;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::GestureScrollBegin(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollBegin, args);
}

void EventSender::GestureScrollEnd(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollEnd, args);
}

void EventSender::GestureScrollUpdate(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollUpdate, args);
}

void EventSender::GestureScrollUpdateWithoutPropagation(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureScrollUpdateWithoutPropagation, args);
}

void EventSender::GestureTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTap, args);
}

void EventSender::GestureTapDown(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTapDown, args);
}

void EventSender::GestureShowPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureShowPress, args);
}

void EventSender::GestureTapCancel(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTapCancel, args);
}

void EventSender::GestureLongPress(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureLongPress, args);
}

void EventSender::GestureLongTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureLongTap, args);
}

void EventSender::GestureTwoFingerTap(gin::Arguments* args) {
  GestureEvent(WebInputEvent::GestureTwoFingerTap, args);
}

void EventSender::ContinuousMouseScrollBy(gin::Arguments* args) {
  WebMouseWheelEvent event;
  InitMouseWheelEvent(args, true, &event);
  view_->handleInputEvent(event);
}

void EventSender::MouseMoveTo(gin::Arguments* args) {
  if (force_layout_on_events_)
    view_->layout();

  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }
  WebPoint mouse_pos(static_cast<int>(x), static_cast<int>(y));

  int modifiers = 0;
  if (!args->PeekNext().IsEmpty())
    modifiers = GetKeyModifiersFromV8(args->PeekNext());

  if (is_drag_mode_ && pressed_button_ == WebMouseEvent::ButtonLeft &&
      !replaying_saved_events_) {
    SavedEvent saved_event;
    saved_event.type = SavedEvent::TYPE_MOUSE_MOVE;
    saved_event.pos = mouse_pos;
    saved_event.modifiers = modifiers;
    mouse_event_queue_.push_back(saved_event);
  } else {
    WebMouseEvent event;
    InitMouseEvent(WebInputEvent::MouseMove,
                   pressed_button_,
                   mouse_pos,
                   GetCurrentEventTimeSec(),
                   click_count_,
                   modifiers,
                   &event);
    DoMouseMove(event);
  }
}

void EventSender::TrackpadScrollBegin() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.phase = blink::WebMouseWheelEvent::PhaseBegan;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::TrackpadScroll(gin::Arguments* args) {
  WebMouseWheelEvent event;
  InitMouseWheelEvent(args, true, &event);
  event.phase = blink::WebMouseWheelEvent::PhaseChanged;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::TrackpadScrollEnd() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.phase = WebMouseWheelEvent::PhaseEnded;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::MouseScrollBy(gin::Arguments* args) {
   WebMouseWheelEvent event;
  InitMouseWheelEvent(args, false, &event);
  view_->handleInputEvent(event);
}

void EventSender::MouseMomentumBegin() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.momentumPhase = WebMouseWheelEvent::PhaseBegan;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::MouseMomentumBegin2(gin::Arguments* args) {
  WebMouseWheelEvent event;
  InitMouseWheelEvent(args, true, &event);
  event.momentumPhase = WebMouseWheelEvent::PhaseBegan;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::MouseMomentumScrollBy(gin::Arguments* args) {
  WebMouseWheelEvent event;
  InitMouseWheelEvent(args, true, &event);
  event.momentumPhase = WebMouseWheelEvent::PhaseChanged;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::MouseMomentumEnd() {
  WebMouseWheelEvent event;
  InitMouseEvent(WebInputEvent::MouseWheel,
                 WebMouseEvent::ButtonNone,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 0,
                 &event);
  event.momentumPhase = WebMouseWheelEvent::PhaseEnded;
  event.hasPreciseScrollingDeltas = true;
  view_->handleInputEvent(event);
}

void EventSender::ScheduleAsynchronousClick(int button_number, int modifiers) {
  delegate_->PostTask(new MouseDownTask(this, button_number, modifiers));
  delegate_->PostTask(new MouseUpTask(this, button_number, modifiers));
}

void EventSender::ScheduleAsynchronousKeyDown(const std::string& code_str,
                                              int modifiers,
                                              KeyLocationCode location) {
  delegate_->PostTask(new KeyDownTask(this, code_str, modifiers, location));
}

double EventSender::GetCurrentEventTimeSec() {
  return (delegate_->GetCurrentTimeInMillisecond() + time_offset_ms_) / 1000.0;
}

void EventSender::DoLeapForward(int milliseconds) {
  time_offset_ms_ += milliseconds;
}

void EventSender::SendCurrentTouchEvent(WebInputEvent::Type type) {
  DCHECK_GT(static_cast<unsigned>(WebTouchEvent::touchesLengthCap),
            touch_points_.size());
  if (force_layout_on_events_)
    view_->layout();

  WebTouchEvent touch_event;
  touch_event.type = type;
  touch_event.modifiers = touch_modifiers_;
  touch_event.cancelable = touch_cancelable_;
  touch_event.timeStampSeconds = GetCurrentEventTimeSec();
  touch_event.touchesLength = touch_points_.size();
  for (size_t i = 0; i < touch_points_.size(); ++i)
    touch_event.touches[i] = touch_points_[i];
  view_->handleInputEvent(touch_event);

  for (size_t i = 0; i < touch_points_.size(); ++i) {
    WebTouchPoint* touch_point = &touch_points_[i];
    if (touch_point->state == WebTouchPoint::StateReleased) {
      touch_points_.erase(touch_points_.begin() + i);
      --i;
    } else
      touch_point->state = WebTouchPoint::StateStationary;
  }
}

void EventSender::GestureEvent(WebInputEvent::Type type,
                               gin::Arguments* args) {
  double x;
  double y;
  if (!args->GetNext(&x) || !args->GetNext(&y)) {
    args->ThrowError();
    return;
  }

  WebGestureEvent event;
  event.type = type;

  switch (type) {
    case WebInputEvent::GestureScrollUpdate:
    case WebInputEvent::GestureScrollUpdateWithoutPropagation:
      event.data.scrollUpdate.deltaX = static_cast<float>(x);
      event.data.scrollUpdate.deltaY = static_cast<float>(y);
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      current_gesture_location_.x =
          current_gesture_location_.x + event.data.scrollUpdate.deltaX;
      current_gesture_location_.y =
          current_gesture_location_.y + event.data.scrollUpdate.deltaY;
      break;
    case WebInputEvent::GestureScrollBegin:
      current_gesture_location_ = WebPoint(x, y);
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    case WebInputEvent::GestureScrollEnd:
    case WebInputEvent::GestureFlingStart:
      event.x = current_gesture_location_.x;
      event.y = current_gesture_location_.y;
      break;
    case WebInputEvent::GestureTap:
    {
      float tap_count = 1;
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.data.tap.tapCount = tap_count;
      event.data.tap.width = width;
      event.data.tap.height = height;
      event.x = x;
      event.y = y;
      break;
    }
    case WebInputEvent::GestureTapUnconfirmed:
      if (!args->PeekNext().IsEmpty()) {
        float tap_count;
        if (!args->GetNext(&tap_count)) {
          args->ThrowError();
          return;
        }
        event.data.tap.tapCount = tap_count;
      } else {
        event.data.tap.tapCount = 1;
      }
      event.x = x;
      event.y = y;
      break;
    case WebInputEvent::GestureTapDown:
    {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
      }
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&height)) {
          args->ThrowError();
          return;
        }
      }
      event.x = x;
      event.y = y;
      event.data.tapDown.width = width;
      event.data.tapDown.height = height;
      break;
    }
    case WebInputEvent::GestureShowPress:
    {
      float width = 30;
      float height = 30;
      if (!args->PeekNext().IsEmpty()) {
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        if (!args->PeekNext().IsEmpty()) {
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
        }
      }
      event.x = x;
      event.y = y;
      event.data.showPress.width = width;
      event.data.showPress.height = height;
      break;
    }
    case WebInputEvent::GestureTapCancel:
      event.x = x;
      event.y = y;
      break;
    case WebInputEvent::GestureLongPress:
      event.x = x;
      event.y = y;
      if (!args->PeekNext().IsEmpty()) {
        float width;
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        event.data.longPress.width = width;
        if (!args->PeekNext().IsEmpty()) {
          float height;
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
          event.data.longPress.height = height;
        }
      }
      break;
    case WebInputEvent::GestureLongTap:
      event.x = x;
      event.y = y;
      if (!args->PeekNext().IsEmpty()) {
        float width;
        if (!args->GetNext(&width)) {
          args->ThrowError();
          return;
        }
        event.data.longPress.width = width;
        if (!args->PeekNext().IsEmpty()) {
          float height;
          if (!args->GetNext(&height)) {
            args->ThrowError();
            return;
          }
          event.data.longPress.height = height;
        }
      }
      break;
    case WebInputEvent::GestureTwoFingerTap:
      event.x = x;
      event.y = y;
      if (!args->PeekNext().IsEmpty()) {
        float first_finger_width;
        if (!args->GetNext(&first_finger_width)) {
          args->ThrowError();
          return;
        }
        event.data.twoFingerTap.firstFingerWidth = first_finger_width;
        if (!args->PeekNext().IsEmpty()) {
          float first_finger_height;
          if (!args->GetNext(&first_finger_height)) {
            args->ThrowError();
            return;
          }
          event.data.twoFingerTap.firstFingerHeight = first_finger_height;
        }
      }
      break;
    default:
      NOTREACHED();
  }

  event.globalX = event.x;
  event.globalY = event.y;
  event.timeStampSeconds = GetCurrentEventTimeSec();

  if (force_layout_on_events_)
    view_->layout();

  bool result = view_->handleInputEvent(event);

  // Long press might start a drag drop session. Complete it if so.
  if (type == WebInputEvent::GestureLongPress && !current_drag_data_.isNull()) {
    WebMouseEvent mouse_event;
    InitMouseEvent(WebInputEvent::MouseDown,
                   pressed_button_,
                   WebPoint(x, y),
                   GetCurrentEventTimeSec(),
                   click_count_,
                   0,
                   &mouse_event);

    FinishDragAndDrop(mouse_event, blink::WebDragOperationNone);
  }
  args->Return(result);
}

void EventSender::UpdateClickCountForButton(
    WebMouseEvent::Button button_type) {
  if ((GetCurrentEventTimeSec() - last_click_time_sec_ <
       kMultipleClickTimeSec) &&
      (!OutsideMultiClickRadius(last_mouse_pos_, last_click_pos_)) &&
      (button_type == last_button_type_)) {
    ++click_count_;
  } else {
    click_count_ = 1;
    last_button_type_ = button_type;
  }
}

void EventSender::InitMouseWheelEvent(gin::Arguments* args,
                                      bool continuous,
                                      WebMouseWheelEvent* event) {
  // Force a layout here just to make sure every position has been
  // determined before we send events (as well as all the other methods
  // that send an event do).
  if (force_layout_on_events_)
    view_->layout();

  double horizontal;
  if (!args->GetNext(&horizontal)) {
    args->ThrowError();
    return;
  }
  double vertical;
  if (!args->GetNext(&vertical)) {
    args->ThrowError();
    return;
  }

  bool paged = false;
  bool has_precise_scrolling_deltas = false;
  int modifiers = 0;
  if (!args->PeekNext().IsEmpty()) {
    args->GetNext(&paged);
    if (!args->PeekNext().IsEmpty()) {
      args->GetNext(&has_precise_scrolling_deltas);
      if (!args->PeekNext().IsEmpty())
        modifiers = GetKeyModifiersFromV8(args->PeekNext());
    }
  }

  InitMouseEvent(WebInputEvent::MouseWheel,
                 pressed_button_,
                 last_mouse_pos_,
                 GetCurrentEventTimeSec(),
                 click_count_,
                 modifiers,
                 event);
  event->wheelTicksX = static_cast<float>(horizontal);
  event->wheelTicksY = static_cast<float>(vertical);
  event->deltaX = event->wheelTicksX;
  event->deltaY = event->wheelTicksY;
  event->scrollByPage = paged;
  event->hasPreciseScrollingDeltas = has_precise_scrolling_deltas;

  if (continuous) {
    event->wheelTicksX /= kScrollbarPixelsPerTick;
    event->wheelTicksY /= kScrollbarPixelsPerTick;
  } else {
    event->deltaX *= kScrollbarPixelsPerTick;
    event->deltaY *= kScrollbarPixelsPerTick;
  }
}

void EventSender::FinishDragAndDrop(const WebMouseEvent& e,
                                     blink::WebDragOperation drag_effect) {
  WebPoint client_point(e.x, e.y);
  WebPoint screen_point(e.globalX, e.globalY);
  current_drag_effect_ = drag_effect;
  if (current_drag_effect_) {
    // Specifically pass any keyboard modifiers to the drop method. This allows
    // tests to control the drop type (i.e. copy or move).
    view_->dragTargetDrop(client_point, screen_point, e.modifiers);
  } else {
    view_->dragTargetDragLeave();
  }
  view_->dragSourceEndedAt(client_point, screen_point, current_drag_effect_);
  view_->dragSourceSystemDragEnded();

  current_drag_data_.reset();
}

void EventSender::DoMouseUp(const WebMouseEvent& e) {
  view_->handleInputEvent(e);

  pressed_button_ = WebMouseEvent::ButtonNone;
  last_click_time_sec_ = e.timeStampSeconds;
  last_click_pos_ = last_mouse_pos_;

  // If we're in a drag operation, complete it.
  if (current_drag_data_.isNull())
    return;

  WebPoint client_point(e.x, e.y);
  WebPoint screen_point(e.globalX, e.globalY);
  FinishDragAndDrop(
      e,
      view_->dragTargetDragOver(
          client_point, screen_point, current_drag_effects_allowed_, 0));
}

void EventSender::DoMouseMove(const WebMouseEvent& e) {
  last_mouse_pos_ = WebPoint(e.x, e.y);

  view_->handleInputEvent(e);

  if (pressed_button_ == WebMouseEvent::ButtonNone ||
      current_drag_data_.isNull()) {
    return;
  }

  WebPoint client_point(e.x, e.y);
  WebPoint screen_point(e.globalX, e.globalY);
  current_drag_effect_ = view_->dragTargetDragOver(
      client_point, screen_point, current_drag_effects_allowed_, 0);
}

void EventSender::ReplaySavedEvents() {
  replaying_saved_events_ = true;
  while (!mouse_event_queue_.empty()) {
    SavedEvent e = mouse_event_queue_.front();
    mouse_event_queue_.pop_front();

    switch (e.type) {
      case SavedEvent::TYPE_MOUSE_MOVE: {
        WebMouseEvent event;
        InitMouseEvent(WebInputEvent::MouseMove,
                       pressed_button_,
                       e.pos,
                       GetCurrentEventTimeSec(),
                       click_count_,
                       e.modifiers,
                       &event);
        DoMouseMove(event);
        break;
      }
      case SavedEvent::TYPE_LEAP_FORWARD:
        DoLeapForward(e.milliseconds);
        break;
      case SavedEvent::TYPE_MOUSE_UP: {
        WebMouseEvent event;
        InitMouseEvent(WebInputEvent::MouseUp,
                       e.button_type,
                       last_mouse_pos_,
                       GetCurrentEventTimeSec(),
                       click_count_,
                       e.modifiers,
                       &event);
        DoMouseUp(event);
        break;
      }
      default:
        NOTREACHED();
    }
  }

  replaying_saved_events_ = false;
}

}  // namespace content
