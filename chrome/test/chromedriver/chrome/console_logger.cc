// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/console_logger.h"

#include <sstream>

#include "base/json/json_writer.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/log.h"
#include "chrome/test/chromedriver/chrome/status.h"

namespace {

bool ConsoleLevelToLogLevel(const std::string& name, Log::Level *out_level) {
  const char* kConsoleLevelNames[] = {
    "debug", "log", "warning", "error"
  };

  for (size_t i = 0; i < arraysize(kConsoleLevelNames); ++i) {
    if (name == kConsoleLevelNames[i]) {
      CHECK(Log::kDebug + i <= Log::kError);
      *out_level = static_cast<Log::Level>(Log::kDebug + i);
      return true;
    }
  }
  return false;
}

}  // namespace

ConsoleLogger::ConsoleLogger(Log* log)
    : log_(log) {}

Status ConsoleLogger::OnConnected(DevToolsClient* client) {
  base::DictionaryValue params;
  return client->SendCommand("Console.enable", params);
}

void ConsoleLogger::OnEvent(
    DevToolsClient* client,
    const std::string& method,
    const base::DictionaryValue& params) {
  if (!StartsWithASCII(method, "Console.messageAdded", true))
    return;

  // If the event has proper structure and fields, log formatted.
  // Else it's a weird message that we don't know how to format, log full JSON.
  const base::DictionaryValue *message_dict = NULL;
  if (params.GetDictionary("message", &message_dict)) {
    std::ostringstream message;
    std::string origin;
    if (message_dict->GetString("url", &origin) && !origin.empty()) {
      message << origin;
    } else if (message_dict->GetString("source", &origin) && !origin.empty()) {
      message << origin;
    } else {
      message << "unknown";
    }

    int line = -1;
    if (message_dict->GetInteger("line", &line)) {
      message << " " << line;
      int column = -1;
      if (message_dict->GetInteger("column", &column)) {
        message << ":" << column;
      }
    } else {
      // No line number, but print anyway, just to maintain the number of
      // fields in the formatted message in case someone wants to parse it.
      message << " -";
    }

    std::string text;
    if (message_dict->GetString("text", &text)) {
      message << " " << text;

      std::string level_name;
      Log::Level level = Log::kLog;
      if (message_dict->GetString("level", &level_name)) {
        if (ConsoleLevelToLogLevel(level_name, &level)) {
          log_->AddEntry(level, message.str());  // Found all expected fields.
          return;
        }
      }
    }
  }

  // Don't know how to format, log full JSON.
  std::string message_json;
  base::JSONWriter::Write(&params, &message_json);
  log_->AddEntry(Log::kWarning, message_json);
}
