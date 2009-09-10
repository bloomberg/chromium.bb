// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if LOGGING

#include "chrome/browser/sync/notifier/communicator/xmpp_log.h"

#include <iomanip>
#include <string>
#include <vector>

#include "chrome/browser/sync/notifier/base/time.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"

namespace notifier {

static bool IsAuthTag(const char* str, size_t len) {
  // Beware that str is not NULL terminated
  if (str[0] == '<' &&
      str[1] == 'a' &&
      str[2] == 'u' &&
      str[3] == 't' &&
      str[4] == 'h' &&
      str[5] <= ' ') {
    std::string tag(str, len);
    if (tag.find("mechanism") != std::string::npos)
      return true;
  }
  return false;
}

static bool IsChatText(const char* str, size_t len) {
  // Beware that str is not NULL terminated
  if (str[0] == '<' &&
      str[1] == 'm' &&
      str[2] == 'e' &&
      str[3] == 's' &&
      str[4] == 's' &&
      str[5] == 'a' &&
      str[6] == 'g' &&
      str[7] == 'e' &&
      str[8] <= ' ') {
    std::string tag(str, len);
    if (tag.find("chat") != std::string::npos)
      return true;
  }
  return false;
}

void XmppLog::XmppPrint(bool output) {
  std::vector<char>* buffer = output ?
      &xmpp_output_buffer_ : &xmpp_input_buffer_;
  const bool log_chat = LOG_CHECK_LEVEL(LS_SENSITIVE);
  if (buffer->size()) {
    char* time_string = GetLocalTimeAsString();
    LOG(INFO) << (output ? "SEND >>>>>>>>>>>>>>>>>>>>>>>>>" :
                  "RECV <<<<<<<<<<<<<<<<<<<<<<<<<")
              << " : " << time_string;

    int start = 0;
    int nest = 3;
    for (int i = 0; i < static_cast<int>(buffer->size()); ++i) {
      if ((*buffer)[i] == '>') {
        bool indent;
        if ((i > 0) && ((*buffer)[i - 1] == '/')) {
          indent = false;
        } else if ((start + 1 < static_cast<int>(buffer->size())) &&
                   ((*buffer)[start + 1] == '/')) {
          indent = false;
          nest -= 2;
        } else {
          indent = true;
        }

        // Output a tag
        LOG(INFO) << std::setw(nest) << " "
                  << std::string(&((*buffer)[start]), i + 1 - start);

        if (indent)
          nest += 2;

        // Note if it's a PLAIN auth tag
        if (IsAuthTag(&((*buffer)[start]), i + 1 - start)) {
          censor_password_ = true;
        } else if (!log_chat && IsChatText(&((*buffer)[start]),
                                           i + 1 - start)) {
          censor_password_ = true;
        }

        start = i + 1;
      }

      if ((*buffer)[i] == '<' && start < i) {
        if (censor_password_) {
          LOG(INFO) << std::setw(nest) << " " << "## TEXT REMOVED ##";
          censor_password_ = false;
        } else {
          LOG(INFO) << std::setw(nest) << " "
                    << std::string(&((*buffer)[start]), i - start);
        }
        start = i;
      }
    }
    buffer->erase(buffer->begin(), buffer->begin() + start);
  }
}
}  // namespace notifier

#endif  // if LOGGING
