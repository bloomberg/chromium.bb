// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_
#define CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_
#pragma once

#include <guiddef.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/win/event_trace_controller.h"

class FilePath;

namespace logging_win {

// A file logger instance captures LOG messages and trace events emitted via
// Event Tracing for Windows (ETW) and sends them to a file.  Events can be
// pulled from the file sometime later with PrintLogFile or ReadLogFile
// (currently in log_file_printer_win.h and log_file_reader_win.h,
// respectively).
//
// Important usage notes (read this):
// - Due to the nature of event generation, only one instance of this class may
//   be initialized at a time.
// - This class is not thread safe.
// - This class uses facilities that require the process to run with admin
//   rights; StartLogging() will return false if this is not the case.
//
// Initializing an instance will add the variable CHROME_ETW_LOGGING=1 to the
// system-wide environment if it is not present.  In the case where it is not
// already present, log messages will not be captured from currently running
// instances of Chrome, Chrome Frame, or other providers that generate events
// conditionally based on that environment variable.
class FileLogger {
 public:
  enum EventProviderBits {
    // Log messages from chrome.exe.
    CHROME_LOG_PROVIDER         = 1 << 0,
    // Log messages from npchrome_frame.dll.
    CHROME_FRAME_LOG_PROVIDER   = 1 << 1,
    // Log messages from the current process.
    CHROME_TESTS_LOG_PROVIDER   = 1 << 2,
    // Trace events.
    CHROME_TRACE_EVENT_PROVIDER = 1 << 3,
  };

  static const uint32 kAllEventProviders = (CHROME_LOG_PROVIDER |
                                            CHROME_FRAME_LOG_PROVIDER |
                                            CHROME_TESTS_LOG_PROVIDER |
                                            CHROME_TRACE_EVENT_PROVIDER);

  FileLogger();
  ~FileLogger();

  // Initializes the instance to collect logs from all supported providers.
  void Initialize();

  // Initializes the instance to collect logs from the providers present in
  // the given mask; see EventProviderBits.
  void Initialize(uint32 event_provider_mask);

  // Starts capturing logs from all providers into |log_file|.  The common file
  // extension for such files is .etl.  Returns false if the session could not
  // be started (e.g., if not running as admin) or if no providers could be
  // enabled.
  bool StartLogging(const FilePath& log_file);

  // Stops capturing logs.
  void StopLogging();

  // Returns true if logs are being captured.
  bool is_logging() const {
    return controller_.session_name() && *controller_.session_name();
  }

 private:
  // A helper for setting/clearing a variable in the system-wide environment.
  class ScopedSystemEnvironmentVariable {
   public:
    ScopedSystemEnvironmentVariable(const string16& variable,
                                    const string16& value);
    ~ScopedSystemEnvironmentVariable();

   private:
    static void NotifyOtherProcesses();

    // Non-empty if the variable was inserted into the system-wide environment.
    string16 variable_;
    DISALLOW_COPY_AND_ASSIGN(ScopedSystemEnvironmentVariable);
  };

  bool EnableProviders();
  void DisableProviders();

  void ConfigureChromeEtwLogging();
  void RevertChromeEtwLogging();

  static bool is_initialized_;

  scoped_ptr<ScopedSystemEnvironmentVariable> etw_logging_configurator_;
  base::win::EtwTraceController controller_;
  uint32 event_provider_mask_;

  DISALLOW_COPY_AND_ASSIGN(FileLogger);
};

}  // namespace logging_win

#endif  // CHROME_TEST_LOGGING_WIN_FILE_LOGGER_H_
