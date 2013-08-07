// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/log_private/log_parser.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_split.h"
#include "chrome/browser/extensions/api/log_private/log_private_api.h"
#include "chrome/common/extensions/api/log_private.h"

using std::string;
using std::vector;

namespace extensions {

LogParser::LogParser() {
}

LogParser::~LogParser() {
}

void LogParser::Parse(
    const string& input,
    std::vector<linked_ptr<api::log_private::LogEntry> >* output,
    FilterHandler* filter_handler) const {
  std::vector<string> entries;
  // Assume there is no newline in the log entry
  base::SplitString(input, '\n', &entries);

  for (size_t i = 0; i < entries.size(); i++) {
    ParseEntry(entries[i], output, filter_handler);
  }
}

}  // namespace extensions
