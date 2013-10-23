// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/crash_upload_list_win.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

CrashUploadListWin::CrashUploadListWin(Delegate* delegate,
                                       const base::FilePath& upload_log_path)
    : CrashUploadList(delegate, upload_log_path) {}

void CrashUploadListWin::LoadUploadList() {
  std::vector<uint8> buffer(1024);
  HANDLE event_log = OpenEventLog(NULL, L"Application");
  if (event_log) {
    ClearUploads();
    while (true) {
      DWORD bytes_read;
      DWORD bytes_needed;
      BOOL success =
          ReadEventLog(event_log,
                       EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ,
                       0,
                       &buffer[0],
                       buffer.size(),
                       &bytes_read,
                       &bytes_needed);
      if (success) {
        DWORD record_offset = 0;
        // The ReadEventLog() API docs imply, but do not explicitly state that
        // partial records will not be returned. Use DCHECK() to affirm this.
        while (record_offset < bytes_read) {
          DCHECK(record_offset + sizeof(EVENTLOGRECORD) <= bytes_read);
          EVENTLOGRECORD* record = (EVENTLOGRECORD*)&buffer[record_offset];
          DCHECK(record_offset + record->Length <= bytes_read);
          if (IsPossibleCrashLogRecord(record))
            ProcessPossibleCrashLogRecord(record);
          record_offset += record->Length;
        }
      } else if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
        // Resize buffer to the required minimum size.
        buffer.resize(bytes_needed);
      } else {
        // Stop on any other error, including the expected case
        // of ERROR_HANDLE_EOF.
        DCHECK(GetLastError() == ERROR_HANDLE_EOF);
        break;
      }
    }
    CloseEventLog(event_log);
  }
}

bool CrashUploadListWin::IsPossibleCrashLogRecord(
    EVENTLOGRECORD* record) const {
  LPWSTR provider_name = (LPWSTR)((uint8*)record + sizeof(EVENTLOGRECORD));
  return !wcscmp(L"Chrome", provider_name) &&
      record->EventType == EVENTLOG_INFORMATION_TYPE &&
      record->NumStrings >= 1;
}

void CrashUploadListWin::ProcessPossibleCrashLogRecord(EVENTLOGRECORD* record) {
  // Add the crash if the message matches the expected pattern.
  const std::wstring pattern_prefix(L"Id=");
  const std::wstring pattern_suffix(L".");
  std::wstring message((LPWSTR)((uint8*)record + record->StringOffset));
  size_t start_index = message.find(pattern_prefix);
  if (start_index != std::wstring::npos) {
    start_index += pattern_prefix.size();
    size_t end_index = message.find(pattern_suffix, start_index);
    if (end_index != std::wstring::npos) {
      std::wstring crash_id =
          message.substr(start_index, end_index - start_index);
      AppendUploadInfo(
          UploadInfo(base::SysWideToUTF8(crash_id),
                     base::Time::FromDoubleT(record->TimeGenerated)));
    }
  }
}
