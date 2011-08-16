// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/mouse_commands.h"

#include "base/values.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/automation/value_conversion_util.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "chrome/test/webdriver/webdriver_basic_types.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace {

const int kLeftButton = 0;
const int kMiddleButton = 1;
const int kRightButton = 2;

}  // namespace

namespace webdriver {

MoveAndClickCommand::MoveAndClickCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebElementCommand(path_segments, parameters) {}

MoveAndClickCommand::~MoveAndClickCommand() {}

bool MoveAndClickCommand::DoesPost() {
  return true;
}

void MoveAndClickCommand::ExecutePost(Response* response) {
  std::string tag_name;
  Error* error = session_->GetElementTagName(
      session_->current_target(), element, &tag_name);
  if (error) {
    response->SetError(error);
    return;
  }

  if (tag_name == "option") {
    const char* kCanOptionBeToggledScript =
        "function(option) {"
        "  var select = option.parentElement;"
        "  if (!select || select.tagName.toLowerCase() != 'select')"
        "    throw new Error('Option element is not in a select');"
        "  return select.multiple;"
        "}";
    bool can_be_toggled;
    error = session_->ExecuteScriptAndParse(
        session_->current_target(),
        kCanOptionBeToggledScript,
        "canOptionBeToggled",
        CreateListValueFrom(element),
        CreateDirectValueParser(&can_be_toggled));
    if (error) {
      response->SetError(error);
      return;
    }

    if (can_be_toggled) {
      error = session_->ToggleOptionElement(
          session_->current_target(), element);
    } else {
      error = session_->SetOptionElementSelected(
          session_->current_target(), element, true);
    }
  } else {
    Point location;
    error = session_->GetClickableLocation(element, &location);
    if (!error)
      error = session_->MouseMoveAndClick(location, automation::kLeftButton);
  }
  if (error) {
    response->SetError(error);
    return;
  }
}

HoverCommand::HoverCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : WebElementCommand(path_segments, parameters) {}

HoverCommand::~HoverCommand() {}

bool HoverCommand::DoesPost() {
  return true;
}

void HoverCommand::ExecutePost(Response* response) {
  Error* error = NULL;
  Point location;
  error = session_->GetClickableLocation(element, &location);
  if (!error)
    error = session_->MouseMove(location);
  if (error) {
    response->SetError(error);
    return;
  }
}

DragCommand::DragCommand(const std::vector<std::string>& path_segments,
                         const DictionaryValue* const parameters)
    : WebElementCommand(path_segments, parameters) {}

DragCommand::~DragCommand() {}

bool DragCommand::Init(Response* const response) {
  if (!WebElementCommand::Init(response))
    return false;

  if (!GetIntegerParameter("x", &drag_x_) ||
      !GetIntegerParameter("y", &drag_y_)) {
    response->SetError(new Error(kBadRequest, "Missing (x,y) coordinates"));
    return false;
  }

  return true;
}

bool DragCommand::DoesPost() {
  return true;
}

void DragCommand::ExecutePost(Response* response) {
  Error* error = NULL;
  Point drag_from;
  error = session_->GetClickableLocation(element, &drag_from);
  if (error) {
    response->SetError(error);
    return;
  }

  Point drag_to(drag_from);
  drag_to.Offset(drag_x_, drag_y_);
  if (drag_to.x() < 0 || drag_to.y() < 0)
    error = new Error(kBadRequest, "Invalid (x,y) coordinates");
  if (!error)
    error = session_->MouseDrag(drag_from, drag_to);

  if (error) {
    response->SetError(error);
    return;
  }
}

AdvancedMouseCommand::AdvancedMouseCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

AdvancedMouseCommand::~AdvancedMouseCommand() {}

bool AdvancedMouseCommand::DoesPost() {
  return true;
}

MoveToCommand::MoveToCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters), has_element_(false),
      has_offset_(false), x_offset_(0), y_offset_(0) {}

MoveToCommand::~MoveToCommand() {}

bool MoveToCommand::Init(Response* const response) {
  if (!AdvancedMouseCommand::Init(response))
    return false;

  std::string element_name;
  has_element_ = GetStringParameter("element", &element_name);

  if (has_element_) {
    element_ = WebElementId(element_name);
  }

  has_offset_ = GetIntegerParameter("xoffset", &x_offset_) &&
      GetIntegerParameter("yoffset", &y_offset_);

  if (!has_element_ && !has_offset_) {
    response->SetError(new Error(
        kBadRequest, "Invalid command arguments"));
    return false;
  }

  return true;
}

void MoveToCommand::ExecutePost(Response* const response) {
  Point location;
  Error* error;

  if (has_element_) {
    // If an element is specified, calculate the coordinate.
    error = session_->GetElementLocationInView(element_, &location);
    if (error) {
      response->SetError(error);
      return;
    }
  } else {
    // If not, use the current mouse position.
    location = session_->get_mouse_position();
  }

  if (has_offset_) {
    // If an offset is specified, translate by the offset.
    location.Offset(x_offset_, y_offset_);
  } else {
    DCHECK(has_element_);

    // If not, calculate the half of the element size and translate by it.
    Size size;
    error = session_->GetElementSize(session_->current_target(),
                                     element_, &size);
    if (error) {
      response->SetError(error);
      return;
    }

    location.Offset(size.width() / 2, size.height() / 2);
  }

  error = session_->MouseMove(location);
  if (error) {
    response->SetError(error);
    return;
  }
}

ClickCommand::ClickCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters) {}

ClickCommand::~ClickCommand() {}

bool ClickCommand::Init(Response* const response) {
  if (!AdvancedMouseCommand::Init(response))
    return false;

  if (!GetIntegerParameter("button", &button_)) {
    response->SetError(new Error(kBadRequest, "Missing mouse button argument"));
    return false;
  }

  return true;
}

void ClickCommand::ExecutePost(Response* const response) {
  // Convert the button argument to a constant value in automation.
  automation::MouseButton button;
  if (button_ == kLeftButton) {
    button = automation::kLeftButton;
  } else if (button_ == kMiddleButton) {
    button = automation::kMiddleButton;
  } else if (button_ == kRightButton) {
    button = automation::kRightButton;
  } else {
    response->SetError(new Error(kBadRequest, "Invalid mouse button"));
    return;
  }

  Error* error = session_->MouseClick(button);
  if (error) {
    response->SetError(error);
    return;
  }
}

ButtonDownCommand::ButtonDownCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters) {}

ButtonDownCommand::~ButtonDownCommand() {}

void ButtonDownCommand::ExecutePost(Response* const response) {
  session_->MouseButtonDown();
}

ButtonUpCommand::ButtonUpCommand(const std::vector<std::string>& path_segments,
                                 const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters) {}

ButtonUpCommand::~ButtonUpCommand() {}

void ButtonUpCommand::ExecutePost(Response* const response) {
  session_->MouseButtonUp();
}

DoubleClickCommand::DoubleClickCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters) {}

DoubleClickCommand::~DoubleClickCommand() {}

void DoubleClickCommand::ExecutePost(Response* const response) {
  Error* error = session_->MouseDoubleClick();
  if (error) {
    response->SetError(error);
    return;
  }
}

}  // namespace webdriver
