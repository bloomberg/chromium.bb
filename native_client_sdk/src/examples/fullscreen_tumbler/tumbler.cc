// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "examples/fullscreen_tumbler/tumbler.h"

#include <stdio.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "examples/fullscreen_tumbler/cube.h"
#include "examples/fullscreen_tumbler/opengl_context.h"
#include "examples/fullscreen_tumbler/scripting_bridge.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/cpp/var.h"

namespace {
const uint32_t kKeyEnter = 0x0D;
const size_t kQuaternionElementCount = 4;
const char* const kArrayStartCharacter = "[";
const char* const kArrayEndCharacter = "]";
const char* const kArrayDelimiter = ",";

// Return the value of parameter named |param_name| from |parameters|.  If
// |param_name| doesn't exist, then return an empty string.
std::string GetParameterNamed(
    const std::string& param_name,
    const tumbler::MethodParameter& parameters) {
  tumbler::MethodParameter::const_iterator i =
      parameters.find(param_name);
  if (i == parameters.end()) {
    return "";
  }
  return i->second;
}

// Convert the JSON string |array| into a vector of floats.  |array| is
// expected to be a string bounded by '[' and ']' and containing a
// comma-delimited list of numbers.  Any errors result in the return of an
// empty array.
std::vector<float> CreateArrayFromJSON(const std::string& json_array) {
  std::vector<float> float_array;
  size_t array_start_pos = json_array.find_first_of(kArrayStartCharacter);
  size_t array_end_pos = json_array.find_last_of(kArrayEndCharacter);
  if (array_start_pos == std::string::npos ||
      array_end_pos == std::string::npos)
    return float_array;  // Malformed JSON: missing '[' or ']'.
  // Pull out the array elements.
  size_t token_pos = array_start_pos + 1;
  while (token_pos < array_end_pos) {
    float_array.push_back(strtof(json_array.data() + token_pos, NULL));
    size_t delim_pos = json_array.find_first_of(kArrayDelimiter, token_pos);
    if (delim_pos == std::string::npos)
      break;
    token_pos = delim_pos + 1;
  }
  return float_array;
}
}  // namespace

namespace tumbler {

Tumbler::Tumbler(PP_Instance instance)
    : pp::Instance(instance),
      full_screen_(this),
      has_focus_(false),
      cube_(NULL) {
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);
}

Tumbler::~Tumbler() {
  // Destroy the cube view while GL context is current.
  opengl_context_->MakeContextCurrent(this);
  delete cube_;
}

bool Tumbler::Init(uint32_t /* argc */,
                   const char* /* argn */[],
                   const char* /* argv */[]) {
  // Add all the methods to the scripting bridge.
  ScriptingBridge::SharedMethodCallbackExecutor set_orientation_method(
      new tumbler::MethodCallback<Tumbler>(
          this, &Tumbler::SetCameraOrientation));
  scripting_bridge_.AddMethodNamed("setCameraOrientation",
                                    set_orientation_method);
  return true;
}

void Tumbler::HandleMessage(const pp::Var& message) {
  if (!message.is_string())
    return;
  scripting_bridge_.InvokeMethod(message.AsString());
}

bool Tumbler::HandleInputEvent(const pp::InputEvent& event) {
  switch (event.GetType()) {
    case PP_INPUTEVENT_TYPE_UNDEFINED:
      break;
    case PP_INPUTEVENT_TYPE_MOUSEDOWN:
      // If we do not yet have focus, return true. In return Chrome will give
      // focus to the NaCl embed.
      return !has_focus_;
      break;
    case PP_INPUTEVENT_TYPE_KEYDOWN:
      HandleKeyDownEvent(pp::KeyboardInputEvent(event));
      break;
    case PP_INPUTEVENT_TYPE_MOUSEUP:
    case PP_INPUTEVENT_TYPE_MOUSEMOVE:
    case PP_INPUTEVENT_TYPE_MOUSEENTER:
    case PP_INPUTEVENT_TYPE_MOUSELEAVE:
    case PP_INPUTEVENT_TYPE_WHEEL:
    case PP_INPUTEVENT_TYPE_RAWKEYDOWN:
    case PP_INPUTEVENT_TYPE_KEYUP:
    case PP_INPUTEVENT_TYPE_CHAR:
    case PP_INPUTEVENT_TYPE_CONTEXTMENU:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_START:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE:
    case PP_INPUTEVENT_TYPE_IME_COMPOSITION_END:
    case PP_INPUTEVENT_TYPE_IME_TEXT:
    default:
      return false;
  }
  return false;
}

void Tumbler::DidChangeView(const pp::Rect& position, const pp::Rect& clip) {
  // Note: When switching to fullscreen, the new View position will be a
  // rectangle that encompasses the entire screen - e.g. 1900x1200 - with its
  // top-left corner at (0, 0). When switching back to the windowed screen the
  // position returns to what it was before going to fullscreen.
  int cube_width = cube_ ? cube_->width() : 0;
  int cube_height = cube_ ? cube_->height() : 0;
  if (position.size().width() == cube_width &&
      position.size().height() == cube_height) {
    return;  // Size didn't change, no need to update anything.
  }

  if (opengl_context_ == NULL)
    opengl_context_.reset(new OpenGLContext(this));
  opengl_context_->InvalidateContext(this);
  opengl_context_->ResizeContext(position.size());
  if (!opengl_context_->MakeContextCurrent(this))
    return;
  if (cube_ == NULL) {
    cube_ = new Cube(opengl_context_);
    cube_->PrepareOpenGL();
  }
  cube_->Resize(position.size().width(), position.size().height());
  DrawSelf();
}

void Tumbler::DidChangeFocus(bool focus) {
  has_focus_ = focus;
}

void Tumbler::DrawSelf() {
  if (cube_ == NULL || opengl_context_ == NULL)
    return;
  opengl_context_->MakeContextCurrent(this);
  cube_->Draw();
  opengl_context_->FlushContext();
}

void Tumbler::HandleKeyDownEvent(const pp::KeyboardInputEvent& key_event) {
  // Pressing the Enter key toggles the view to/from full screen.
  if (key_event.GetKeyCode() == kKeyEnter) {
    if (!full_screen_.IsFullscreen()) {
      if (!full_screen_.SetFullscreen(true)) {
        printf("Failed to switch to fullscreen mode.\n");
      }
    } else {
      if (!full_screen_.SetFullscreen(false)) {
        printf("Failed to switch to normal mode.\n");
      }
    }
  }
}

void Tumbler::SetCameraOrientation(
    const tumbler::ScriptingBridge& bridge,
    const tumbler::MethodParameter& parameters) {
  // |parameters| is expected to contain one object named "orientation", whose
  // value is a JSON string that represents an array of four floats.
  if (parameters.size() != 1 || cube_ == NULL)
    return;
  std::string orientation_desc = GetParameterNamed("orientation", parameters);
  std::vector<float> orientation = CreateArrayFromJSON(orientation_desc);
  if (orientation.size() != kQuaternionElementCount) {
    return;
  }
  cube_->SetOrientation(orientation);
  DrawSelf();
}

}  // namespace tumbler

