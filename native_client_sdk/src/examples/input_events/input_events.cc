// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// C headers
#include <cassert>
#include <cstdio>

// C++ headers
#include <sstream>
#include <string>

// NaCl
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/var.h"

namespace {
const char* const kDidChangeView = "DidChangeView";
const char* const kHandleInputEvent = "DidHandleInputEvent";
const char* const kDidChangeFocus = "DidChangeFocus";
const char* const kHaveFocus = "HaveFocus";
const char* const kDontHaveFocus = "DontHaveFocus";

// Convert a given modifier to a descriptive string.  Note that the actual
// declared type of modifier in each of the event classes is uint32_t, but it is
// expected to be interpreted as a bitfield of 'or'ed PP_InputEvent_Modifier
// values.
std::string ModifierToString(uint32_t modifier) {
  std::string s;
  if (modifier & PP_INPUTEVENT_MODIFIER_SHIFTKEY) {
    s += "shift ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CONTROLKEY) {
    s += "ctrl ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ALTKEY) {
    s += "alt ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_METAKEY) {
    s += "meta ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISKEYPAD) {
    s += "keypad ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT) {
    s += "autorepeat ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN) {
    s += "left-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN) {
    s += "middle-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN) {
    s += "right-button-down ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY) {
    s += "caps-lock ";
  }
  if (modifier & PP_INPUTEVENT_MODIFIER_NUMLOCKKEY) {
    s += "num-lock ";
  }
  return s;
}

std::string MouseButtonToString(PP_InputEvent_MouseButton button) {
  switch (button) {
    case PP_INPUTEVENT_MOUSEBUTTON_NONE:
      return "None";
    case PP_INPUTEVENT_MOUSEBUTTON_LEFT:
      return "Left";
    case PP_INPUTEVENT_MOUSEBUTTON_MIDDLE:
      return "Middle";
    case PP_INPUTEVENT_MOUSEBUTTON_RIGHT:
      return "Right";
    default:
      std::ostringstream stream;
      stream << "Unrecognized ("
             << static_cast<int32_t>(button)
             << ")";
      return stream.str();
  }
}

}  // namespace

class EventInstance : public pp::Instance {
 public:
  explicit EventInstance(PP_Instance instance)
      : pp::Instance(instance) {
    RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL);
    RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
  }
  virtual ~EventInstance() {}

  /// Clicking outside of the instance's bounding box
  /// will create a DidChangeFocus event (the NaCl instance is
  /// out of focus). Clicking back inside the instance's
  /// bounding box will create another DidChangeFocus event
  /// (the NaCl instance is back in focus). The default is
  /// that the instance is out of focus.
  void DidChangeFocus(bool focus) {
    PostMessage(pp::Var(kDidChangeFocus));
    if (focus == true) {
      PostMessage(pp::Var(kHaveFocus));
    } else {
      PostMessage(pp::Var(kDontHaveFocus));
    }
  }

  /// Scrolling the mouse wheel causes a DidChangeView event.
  void DidChangeView(const pp::Rect& position,
                     const pp::Rect& clip) {
    PostMessage(pp::Var(kDidChangeView));
  }

  void GotKeyEvent(const pp::KeyboardInputEvent& key_event,
                   const std::string& kind) {
    std::ostringstream stream;
    stream << pp_instance() << ":"
           << " Key event:" << kind
           << " modifier:" << ModifierToString(key_event.GetModifiers())
           << " key_code:" << key_event.GetKeyCode()
           << " time:" << key_event.GetTimeStamp()
           << " text:" << key_event.GetCharacterText().DebugString()
           << "\n";
    PostMessage(stream.str());
  }

  void GotMouseEvent(const pp::MouseInputEvent& mouse_event,
                     const std::string& kind) {
    std::ostringstream stream;
    stream << pp_instance() << ":"
           << " Mouse event:" << kind
           << " modifier:" << ModifierToString(mouse_event.GetModifiers())
           << " button:" << MouseButtonToString(mouse_event.GetButton())
           << " x:" << mouse_event.GetPosition().x()
           << " y:" << mouse_event.GetPosition().y()
           << " click_count:" << mouse_event.GetClickCount()
           << " time:" << mouse_event.GetTimeStamp()
           << "\n";
    PostMessage(stream.str());
  }

  void GotWheelEvent(const pp::WheelInputEvent& wheel_event) {
    std::ostringstream stream;
    stream << pp_instance() << ": Wheel event."
           << " modifier:" << ModifierToString(wheel_event.GetModifiers())
           << " deltax:" << wheel_event.GetDelta().x()
           << " deltay:" << wheel_event.GetDelta().y()
           << " wheel_ticks_x:" << wheel_event.GetTicks().x()
           << " wheel_ticks_y:"<< wheel_event.GetTicks().y()
           << " scroll_by_page: "
           << (wheel_event.GetScrollByPage() ? "true" : "false")
           << "\n";
    PostMessage(stream.str());
  }

  // Handle an incoming input event by switching on type and dispatching
  // to the appropriate subtype handler.
  //
  // HandleInputEvent operates on the main Pepper thread. In large
  // real-world applications, you'll want to create a separate thread
  // that puts events in a queue and handles them independant of the main
  // thread so as not to slow down the browser. There is an additional
  // version of this example in the examples directory that demonstrates
  // this best practice.
  virtual bool HandleInputEvent(const pp::InputEvent& event) {
    PostMessage(pp::Var(kHandleInputEvent));
    switch (event.GetType()) {
      case PP_INPUTEVENT_TYPE_UNDEFINED:
        break;
      case PP_INPUTEVENT_TYPE_MOUSEDOWN:
        GotMouseEvent(pp::MouseInputEvent(event), "Down");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEUP:
        GotMouseEvent(pp::MouseInputEvent(event), "Up");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEMOVE:
        GotMouseEvent(pp::MouseInputEvent(event), "Move");
        break;
      case PP_INPUTEVENT_TYPE_MOUSEENTER:
        GotMouseEvent(pp::MouseInputEvent(event), "Enter");
        break;
      case PP_INPUTEVENT_TYPE_MOUSELEAVE:
        GotMouseEvent(pp::MouseInputEvent(event), "Leave");
        break;
      case PP_INPUTEVENT_TYPE_WHEEL:
        GotWheelEvent(pp::WheelInputEvent(event));
        break;
      case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
        GotKeyEvent(pp::KeyboardInputEvent(event), "RawKeyDown");
        break;
      case PP_INPUTEVENT_TYPE_KEYDOWN:
        GotKeyEvent(pp::KeyboardInputEvent(event), "Down");
        break;
      case PP_INPUTEVENT_TYPE_KEYUP:
        GotKeyEvent(pp::KeyboardInputEvent(event), "Up");
        break;
      case PP_INPUTEVENT_TYPE_CHAR:
        GotKeyEvent(pp::KeyboardInputEvent(event), "Character");
        break;
      case PP_INPUTEVENT_TYPE_CONTEXTMENU:
        GotKeyEvent(pp::KeyboardInputEvent(event), "Context");
        break;
      // Note that if we receive an IME event we just send a message back
      // to the browser to indicate we have received it.
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
        PostMessage(pp::Var("PP_INPUTEVENT_TYPE_IME_COMPOSITION_START"));
        break;
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
        PostMessage(pp::Var("PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE"));
        break;
      case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
        PostMessage(pp::Var("PP_INPUTEVENT_TYPE_IME_COMPOSITION_END"));
        break;
      case PP_INPUTEVENT_TYPE_IME_TEXT:
        PostMessage(pp::Var("PP_INPUTEVENT_TYPE_IME_COMPOSITION_TEXT"));
        break;
      default:
        assert(false);
        return false;
    }
    return true;
  }
};

// The EventModule provides an implementation of pp::Module that creates
// EventInstance objects when invoked.  This is part of the glue code that makes
// our example accessible to ppapi.
class EventModule : public pp::Module {
 public:
  EventModule() : pp::Module() {}
  virtual ~EventModule() {}

  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new EventInstance(instance);
  }
};

// Implement the required pp::CreateModule function that creates our specific
// kind of Module (in this case, EventModule).  This is part of the glue code
// that makes our example accessible to ppapi.
namespace pp {
  Module* CreateModule() {
    return new EventModule();
  }
}
