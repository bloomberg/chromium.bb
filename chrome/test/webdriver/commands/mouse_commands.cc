// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/mouse_commands.h"

#include "base/values.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace {

const int kLeftButton = 0;
const int kMiddleButton = 1;
const int kRightButton = 2;

}  // namespace

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
  Error* error = session_->CheckElementPreconditionsForClicking(element);
  if (error) {
    response->SetError(error);
    return;
  }

  gfx::Point location;
  error = session_->GetElementLocationInView(element, &location);
  if (error) {
    response->SetError(error);
    return;
  }

  gfx::Size size;
  error = session_->GetElementSize(session_->current_target(), element, &size);
  if (error) {
    response->SetError(error);
    return;
  }

  location.Offset(size.width() / 2, size.height() / 2);
  error = Action(location);
  if (error) {
    response->SetError(error);
    return;
  }
}

MoveAndClickCommand::MoveAndClickCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : ElementMouseCommand(path_segments, parameters) {}

MoveAndClickCommand::~MoveAndClickCommand() {}

Error* MoveAndClickCommand::Action(const gfx::Point& location) {
  return session_->MouseMoveAndClick(location, automation::kLeftButton);
}

HoverCommand::HoverCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : ElementMouseCommand(path_segments, parameters) {}

HoverCommand::~HoverCommand() {}

Error* HoverCommand::Action(const gfx::Point& location) {
  return session_->MouseMove(location);
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
    response->SetError(new Error(kBadRequest, "Missing (x,y) coordinates"));
    return false;
  }

  return true;
}

Error* DragCommand::Action(const gfx::Point& location) {
  gfx::Point drag_from(location);
  gfx::Point drag_to(drag_from);
  drag_to.Offset(drag_x_, drag_y_);
  if (drag_to.x() < 0 || drag_to.y() < 0)
    return new Error(kBadRequest, "Invalid (x,y) coordinates");

  return session_->MouseDrag(location, drag_to);
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
  gfx::Point location;

  if (has_element_) {
    // If an element is specified, calculate the coordinate.
    Error* error = session_->CheckElementPreconditionsForClicking(element_);
    if (error) {
      response->SetError(error);
      return;
    }

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
    gfx::Size size;
    Error* error = session_->GetElementSize(session_->current_target(),
                                            element_, &size);
    if (error) {
      response->SetError(error);
      return;
    }

    location.Offset(size.width() / 2, size.height() / 2);
  }

  Error* error = session_->MouseMove(location);
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
