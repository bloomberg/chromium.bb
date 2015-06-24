// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/console_message_level.h"

namespace chromeos {
namespace launcher_search_provider {

ErrorReporter::ErrorReporter(content::RenderFrameHost* host) : host_(host) {
}

ErrorReporter::~ErrorReporter() {
}

void ErrorReporter::Warn(const std::string& message) {
  DCHECK(host_);

  host_->AddMessageToConsole(
      content::ConsoleMessageLevel::CONSOLE_MESSAGE_LEVEL_WARNING, message);
}

scoped_ptr<ErrorReporter> ErrorReporter::Duplicate() {
  return make_scoped_ptr(new ErrorReporter(host_));
}

}  // namespace launcher_search_provider
}  // namespace chromeos
