// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/launcher_search_provider/error_reporter.h"

#include "content/public/common/console_message_level.h"
#include "extensions/common/extension_messages.h"

namespace chromeos {
namespace launcher_search_provider {

ErrorReporter::ErrorReporter(IPC::Sender* sender, const int routing_id)
    : sender_(sender), routing_id_(routing_id) {
}

ErrorReporter::~ErrorReporter() {
}

void ErrorReporter::Warn(const std::string& message) {
  DCHECK(sender_);

  sender_->Send(new ExtensionMsg_AddMessageToConsole(
      routing_id_, content::ConsoleMessageLevel::CONSOLE_MESSAGE_LEVEL_WARNING,
      message));
}

scoped_ptr<ErrorReporter> ErrorReporter::Duplicate() {
  return make_scoped_ptr(new ErrorReporter(sender_, routing_id_));
}

}  // namespace launcher_search_provider
}  // namespace chromeos
