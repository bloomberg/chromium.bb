// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <stddef.h>
#include <time.h>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chrome/tools/crash_service/caps/logger_win.h"
#include "components/version_info/version_info_values.h"

namespace {
// Every message has this structure:
// <index>~ <tick_count> <message> ~\n"
const char kMessageHeader[] = "%03d~ %08x %s ~\n";

// Do not re-order these messages. Only add at the end.
const char* kMessages[] {
  "start pid(%lu) version(%s) time(%s)",                    // 0
  "exit pid(%lu) time(%s)",                                 // 1
  "instance found",                                         // 2
};

bool WriteLogLine(HANDLE file, const std::string& txt) {
  if (txt.empty())
    return true;
  DWORD written;
  auto rc = ::WriteFile(file, txt.c_str(),
                        static_cast<DWORD>(txt.size()),
                        &written,
                        nullptr);
  return (rc == TRUE);
}

// Uses |kMessages| at |index| above to write to disk a formatted log line.
bool WriteFormattedLogLine(HANDLE file, size_t index, ...) {
  va_list ap;
  va_start(ap, index);
  auto fmt = base::StringPrintf(
      kMessageHeader, index, ::GetTickCount(), kMessages[index]);
  auto msg = base::StringPrintV(fmt.c_str(), ap);
  auto rc = WriteLogLine(file, msg.c_str());
  va_end(ap);
  return rc;
}

// Returns the current time and date formatted in standard C style.
std::string DateTime() {
  time_t current_time = 0;
  time(&current_time);
  struct tm local_time = {0};
  char time_buf[26] = {0};
  localtime_s(&local_time, &current_time);
  asctime_s(time_buf, &local_time);
  time_buf[24] = '\0';
  return time_buf;
}

}  // namespace

namespace caps {

Logger::Logger(const base::FilePath& path) : file_(INVALID_HANDLE_VALUE) {
  auto logfile = path.Append(L"caps_log.txt");
  // Opening a file like so allows atomic appends, but can't be used
  // for anything else.
  DWORD kShareAll = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  file_ = ::CreateFile(logfile.value().c_str(),
                       FILE_APPEND_DATA, kShareAll,
                       nullptr,
                       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
  if (file_ == INVALID_HANDLE_VALUE)
    return;
  WriteFormattedLogLine(
      file_, 0, ::GetCurrentProcessId(), PRODUCT_VERSION, DateTime().c_str());
}

Logger::~Logger() {
  if (file_ != INVALID_HANDLE_VALUE) {
    WriteFormattedLogLine(
        file_, 1, ::GetCurrentProcessId(), DateTime().c_str());
    ::CloseHandle(file_);
  }
}

}  // namespace caps

