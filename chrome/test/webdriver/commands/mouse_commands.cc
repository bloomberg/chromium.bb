// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/mouse_commands.h"

#include "base/values.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace {
static const int kLeftButton = 0;
static const int kMiddleButton = 1;
static const int kRightButton = 2;
}

namespace webdriver {

ElementMouseCommand::ElementMouseCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementMouseCommand::~ElementMouseCommand() {}

bool ElementMouseCommand::DoesPost() {
  return true;
}

void ElementMouseCommand::ExecutePost(Response* response) {
  bool is_displayed;
  ErrorCode code = session_->IsElementDisplayed(
      session_->current_target(), element, &is_displayed);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to determine element visibility",
                        code);
    return;
  }
  if (!is_displayed) {
    SET_WEBDRIVER_ERROR(response, "Element must be displayed",
                        kElementNotVisible);
    return;
  }

  gfx::Point location;
  code = session_->GetElementLocationInView(element, &location);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to compute element location.",
                        code);
    return;
  }

  gfx::Size size;
  code = session_->GetElementSize(session_->current_target(), element, &size);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to compute element size.", code);
    return;
  }

  location.Offset(size.width() / 2, size.height() / 2);
  if (!Action(location, response))
    return;

  response->SetStatus(kSuccess);
}

MoveAndClickCommand::MoveAndClickCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : ElementMouseCommand(path_segments, parameters) {}

MoveAndClickCommand::~MoveAndClickCommand() {}

bool MoveAndClickCommand::Action(const gfx::Point& location,
                                 Response* const response) {
  VLOG(1) << "Mouse click at: (" << location.x() << ", "
          << location.y() << ")" << std::endl;
  if (!session_->MouseMoveAndClick(location, automation::kLeftButton)) {
    SET_WEBDRIVER_ERROR(response, "Performing mouse operation failed",
                        kUnknownError);
    return false;
  }

  return true;
}

HoverCommand::HoverCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : ElementMouseCommand(path_segments, parameters) {}

HoverCommand::~HoverCommand() {}

bool HoverCommand::Action(const gfx::Point& location,
                          Response* const response) {
  VLOG(1) << "Mouse hover at: (" << location.x() << ", "
          << location.y() << ")" << std::endl;
  if (!session_->MouseMove(location)) {
    SET_WEBDRIVER_ERROR(response, "Performing mouse operation failed",
                        kUnknownError);
    return false;
  }

  return true;
}

DragCommand::DragCommand(const std::vector<std::string>& path_segments,
                         const DictionaryValue* const parameters)
    : ElementMouseCommand(path_segments, parameters) {}

DragCommand::~DragCommand() {}

bool DragCommand::Init(Response* const response) {
  if (!ElementMouseCommand::Init(response))
    return false;

  if (!GetIntegerParameter("x", &drag_x_) ||
      !GetIntegerParameter("y", &drag_y_)) {
    SET_WEBDRIVER_ERROR(response,
                        "Request is missing the required positional data",
                        kBadRequest);
    return false;
  }

  return true;
}

bool DragCommand::Action(const gfx::Point& location, Response* const response) {
  gfx::Point drag_from(location);
  gfx::Point drag_to(drag_from);
  drag_to.Offset(drag_x_, drag_y_);
  if (drag_to.x() < 0 || drag_to.y() < 0) {
    SET_WEBDRIVER_ERROR(response, "Invalid pos to drag to", kBadRequest);
    return false;
  }

  // click on the element
  VLOG(1) << "Dragging mouse from: "
          << "(" << location.x() << ", " << location.y() << ") "
          << "to: (" << drag_to.x() << ", " << drag_to.y() << ")"
          << std::endl;
  if (!session_->MouseDrag(location, drag_to)) {
    SET_WEBDRIVER_ERROR(response,
                        "Request is missing the required positional data",
                        kBadRequest);
    return false;
  }

  return true;
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
    SET_WEBDRIVER_ERROR(response,
                        "Request is missing the required data",
                        kBadRequest);
    return false;
  }

  return true;
}

void MoveToCommand::ExecutePost(Response* const response) {
  gfx::Point location;

  if (has_element_) {
    // If an element is specified, calculate the coordinate.
    bool is_displayed;
    ErrorCode code = session_->IsElementDisplayed(session_->current_target(),
                                                  element_, &is_displayed);
    if (code != kSuccess) {
      SET_WEBDRIVER_ERROR(response, "Failed to determine element visibility",
                          code);
      return;
    }

    if (!is_displayed) {
      SET_WEBDRIVER_ERROR(response, "Element must be displayed",
                          kElementNotVisible);
      return;
    }

    code = session_->GetElementLocationInView(element_, &location);
    if (code != kSuccess) {
      SET_WEBDRIVER_ERROR(response, "Failed to compute element location.",
                          code);
      return;
    }

    VLOG(1) << "An element requested. x:" << location.x() << " y:"
            << location.y();

  } else {
    // If not, use the current mouse position.
    VLOG(1) << "No element requested, using current mouse position";
    location = session_->get_mouse_position();
  }

  if (has_offset_) {
    // If an offset is specified, translate by the offset.
    location.Offset(x_offset_, y_offset_);

    VLOG(1) << "An offset requested. x:" << x_offset_ << " y:" << y_offset_;

  } else {
    DCHECK(has_element_);

    // If not, calculate the half of the element size and translate by it.
    gfx::Size size;
    ErrorCode code = session_->GetElementSize(session_->current_target(),
                                              element_, &size);
    if (code != kSuccess) {
      SET_WEBDRIVER_ERROR(response, "Failed to compute element size.", code);
      return;
    }

    location.Offset(size.width() / 2, size.height() / 2);

    VLOG(1) << "No offset requested. The element size is " << size.width()
            << "x" << size.height();
  }

  VLOG(1) << "Mouse moved to: (" << location.x() << ", " << location.y() << ")";
  session_->MouseMove(location);
}

ClickCommand::ClickCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : AdvancedMouseCommand(path_segments, parameters) {}

ClickCommand::~ClickCommand() {}

bool ClickCommand::Init(Response* const response) {
  if (!AdvancedMouseCommand::Init(response))
    return false;

  if (!GetIntegerParameter("button", &button_)) {
    SET_WEBDRIVER_ERROR(response,
                        "Request is missing the required button data",
                        kBadRequest);
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
    SET_WEBDRIVER_ERROR(response, "Invalid button", kBadRequest);
    return;
  }

  session_->MouseClick(button);
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
  session_->MouseDoubleClick();
}

}  // namespace webdriver
