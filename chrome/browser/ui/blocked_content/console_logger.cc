// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/console_logger.h"

#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"

ConsoleLogger::ConsoleLogger() = default;
ConsoleLogger::~ConsoleLogger() = default;

void ConsoleLogger::LogInFrame(content::RenderFrameHost* render_frame_host,
                               content::ConsoleMessageLevel level,
                               const std::string& message) {
  DCHECK(render_frame_host);
  render_frame_host->AddMessageToConsole(level, message);
}
