// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/mouse_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/commands/response.h"
#include "ui/base/events.h"
#include "ui/gfx/point.h"

namespace webdriver {

MouseCommand::~MouseCommand() {}

bool MouseCommand::DoesPost() {
  return true;
}

void MouseCommand::ExecutePost(Response* response) {
  // TODO(jmikhail): verify that the element is visible
  int x, y;
  if (!GetElementLocation(true, &x, &y)) {
    SET_WEBDRIVER_ERROR(response, "Failed to compute element location.",
                        kNoSuchElement);
    return;
  }

  int width, height;
  if (!GetElementSize(&width, &height)) {
    SET_WEBDRIVER_ERROR(response, "Failed to compute element size.",
                       kInternalServerError);
    return;
  }

  x += width / 2;
  y += height / 2;

  switch (cmd_) {
    case kClick:
      VLOG(1) << "Mouse click at: (" << x << ", " << y << ")" << std::endl;
      session_->MouseClick(gfx::Point(x, y), ui::EF_LEFT_BUTTON_DOWN);
      break;

    case kHover:
      VLOG(1) << "Mouse hover at: (" << x << ", " << y << ")" << std::endl;
      session_->MouseMove(gfx::Point(x, y));
      break;

    case kDrag: {
      const int x2 = x + drag_x_;
      const int y2 = y + drag_y_;
      if (x2 < 0 || y2 < 0) {
        SET_WEBDRIVER_ERROR(response, "Invalid pos to drag to", kBadRequest);
        return;
      }

      // click on the element
      VLOG(1) << "Dragging mouse from: (" << x << ", " << y << ") "
              << "to: (" << x2 << ", " << y2 << std::endl;
      session_->MouseDrag(gfx::Point(x, y), gfx::Point(x2, y2));
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

