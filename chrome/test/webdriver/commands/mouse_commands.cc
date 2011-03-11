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

namespace webdriver {

MouseCommand::~MouseCommand() {}

bool MouseCommand::DoesPost() {
  return true;
}

void MouseCommand::ExecutePost(Response* response) {
  // TODO(jmikhail): verify that the element is visible
  gfx::Point location;
  ErrorCode code = session_->GetElementLocationInView(element, &location);
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
  switch (cmd_) {
    case kClick:
      VLOG(1) << "Mouse click at: (" << location.x() << ", "
              << location.y() << ")" << std::endl;
      session_->MouseClick(location, automation::kLeftButton);
      break;

    case kHover:
      VLOG(1) << "Mouse hover at: (" << location.x() << ", "
              << location.y() << ")" << std::endl;
      session_->MouseMove(location);
      break;

    case kDrag: {
      gfx::Point drag_to(location);
      drag_to.Offset(drag_x_, drag_y_);
      if (drag_to.x() < 0 || drag_to.y() < 0) {
        SET_WEBDRIVER_ERROR(response, "Invalid pos to drag to", kBadRequest);
        return;
      }

      // click on the element
      VLOG(1) << "Dragging mouse from: "
              << "(" << location.x() << ", " << location.y() << ") "
              << "to: (" << drag_to.x() << ", " << drag_to.y() << ")"
              << std::endl;
      session_->MouseDrag(location, drag_to);
      break;
    }

    default:
      SET_WEBDRIVER_ERROR(response, "Unknown mouse request", kUnknownCommand);
      return;
  }

  response->SetStatus(kSuccess);
}

DragCommand::DragCommand(const std::vector<std::string>& path_segments,
                         const DictionaryValue* const parameters)
    : MouseCommand(path_segments, parameters, kDrag) {}

DragCommand::~DragCommand() {}

bool DragCommand::Init(Response* const response) {
  if (WebDriverCommand::Init(response)) {
    if (!GetIntegerParameter("x", &drag_x_) ||
        !GetIntegerParameter("y", &drag_y_)) {
      SET_WEBDRIVER_ERROR(response,
                          "Request is missing the required positional data",
                          kBadRequest);
      return false;
    }
    return true;
  }

  return false;
}

ClickCommand::ClickCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : MouseCommand(path_segments, parameters, kClick) {}

ClickCommand::~ClickCommand() {}

HoverCommand::HoverCommand(const std::vector<std::string>& path_segments,
                           const DictionaryValue* const parameters)
    : MouseCommand(path_segments, parameters, kHover) {}

HoverCommand::~HoverCommand() {}

}  // namespace webdriver
