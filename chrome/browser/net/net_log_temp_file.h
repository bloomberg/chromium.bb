// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_LOG_TEMP_FILE_H_
#define CHROME_BROWSER_NET_NET_LOG_TEMP_FILE_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

class ChromeNetLog;
class NetLogLogger;

// NetLogTempFile logs all the NetLog entries into a temporary file
// "chrome-net-export-log.json" created in file_util::GetTempDir() directory.
//
// NetLogTempFile maintains the current state (state_) of the logging into a
// chrome-net-export-log.json file.
//
// The following are the possible states
// a) Only Start is allowed (state_ == STATE_UNINITIALIZED).
// b) Only Stop is allowed (state_ == STATE_ALLOW_STOP).
// c) Either Send or Start is allowed (state_ == STATE_ALLOW_START_SEND).
//
// This is created/destroyed on the UI thread, but all other function calls
// occur on the FILE_USER_BLOCKING thread.
//
// This relies on the UI thread outlasting all other named threads for thread
// safety.
class NetLogTempFile {
 public:
  // This enum lists the UI button commands it could receive.
  enum Command {
    DO_START,  // Call StartLog.
    DO_STOP,   // Call StopLog.
  };

  virtual ~NetLogTempFile();  // Destructs a NetLogTempFile.

  // Accepts the button command and executes it.
  void ProcessCommand(Command command);

  // Returns true and the path to the temporary file. If there is no file to
  // send, then it returns false. It also returns false when actively logging to
  // the file.
  bool GetFilePath(base::FilePath* path);

  // Creates a Value summary of the state of the NetLogTempFile. The caller is
  // responsible for deleting the returned value.
  base::DictionaryValue* GetState();

 protected:
  // Constructs a NetLogTempFile. Only one instance is created in browser
  // process.
  explicit NetLogTempFile(ChromeNetLog* chrome_net_log);

  // Returns path name to file_util::GetTempDir() directory. Returns false if
  // file_util::GetTempDir() fails.
  virtual bool GetNetExportLogDirectory(base::FilePath* path);

  // Returns true if |log_path_| exists.
  virtual bool NetExportLogExists();

 private:
  friend class ChromeNetLog;
  friend class NetLogTempFileTest;

  // Allow tests to access our innards for testing purposes.
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitFailure);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitAllowStart);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, EnsureInitAllowStartOrSend);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, ProcessCommandDoStartAndStop);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, DoStartClearsFile);
  FRIEND_TEST_ALL_PREFIXES(NetLogTempFileTest, CheckAddEvent);

  // This enum lists the possible state NetLogTempFile could be in. It is used
  // to enable/disable "Start", "Stop" and "Send" (email) UI actions.
  enum State {
    STATE_UNINITIALIZED,
    STATE_ALLOW_START,       // Only DO_START Command is allowed.
    STATE_ALLOW_STOP,        // Only DO_STOP Command is allowed.
    STATE_ALLOW_START_SEND,  // Either DO_START or DO_SEND is allowed.
  };

  // Initializes the |state_| to either STATE_ALLOW_START (if there is no
  // temporary file from earlier run) or STATE_ALLOW_START_SEND (if there is a
  // temporary file from earlier run). Returns false if initialization of
  // |log_path_| fails.
  bool EnsureInit();

  // Start collecting NetLog data into chrome-net-export-log.json file in
  // file_util::GetTempDir() directory. It is a no-op if we are already
  // collecting data into a file.
  void StartNetLog();

  // Stop collecting NetLog data into the temporary file. It is a no-op if we
  // are not collecting data into a file.
  void StopNetLog();

  // Updates |log_path_| with base::FilePath to |log_filename_| in the
  // file_util::GetTempDir() directory. Returns false if file_util::GetTempDir()
  // fails.
  bool GetNetExportLog();

  // Helper function for unit tests.
  State state() const { return state_; }

  State state_;  // Current state of NetLogTempFile.

  // Name of the file. It defaults to chrome-net-export-log.json, but can be
  // overwritten by unit tests.
  base::FilePath::StringType log_filename_;

  base::FilePath log_path_;  // base::FilePath to the temporary file.

  // |net_log_logger_| watches the NetLog event stream, and sends all entries to
  // the file created in StartNetLog().
  scoped_ptr<NetLogLogger> net_log_logger_;

  // The |chrome_net_log_| is owned by the browser process, cached here to avoid
  // using global (g_browser_process).
  ChromeNetLog* chrome_net_log_;

  DISALLOW_COPY_AND_ASSIGN(NetLogTempFile);
};

#endif  // CHROME_BROWSER_NET_NET_LOG_TEMP_FILE_H_
