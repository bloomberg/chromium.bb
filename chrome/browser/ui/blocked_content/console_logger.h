// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_CONSOLE_LOGGER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_CONSOLE_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "content/public/common/console_message_level.h"

namespace content {
class RenderFrameHost;
}

// This simple class just forwards console logging to the associated
// RenderFrameHost, to send down to the renderer. It exists for unit tests to
// mock out, allowing unit tests to expect console messages without having to
// write a full browser test.
class ConsoleLogger {
 public:
  ConsoleLogger();
  virtual ~ConsoleLogger();

  virtual void LogInFrame(content::RenderFrameHost* render_frame_host,
                          content::ConsoleMessageLevel level,
                          const std::string& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsoleLogger);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_CONSOLE_LOGGER_H_
