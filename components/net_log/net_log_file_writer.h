// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_NET_LOG_FILE_WRITER_H_
#define COMPONENTS_NET_LOG_NET_LOG_FILE_WRITER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/log/net_log_capture_mode.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
class Value;
}

namespace net {
class FileNetLogObserver;
class URLRequestContextGetter;
}

namespace net_log {

class ChromeNetLog;

// NetLogFileWriter is used exclusively as a support class for net-export.
// It's a singleton that acts as the interface to all NetExportMessageHandlers
// which can tell it to start or stop logging in response to user actions from
// net-export UIs. Because it's a singleton, the logging state can be shared
// between multiple instances of the net-export UI. Internally, it manages an
// instance of net::FileNetLogObserver and handles the attaching/detaching of it
// to the ChromeNetLog. This class is used by the iOS and non-iOS
// implementations of net-export.
//
// NetLogFileWriter maintains the current logging state (using the members
// (|state_|, |log_exists_|, |log_capture_mode_known_|, |log_capture_mode_|).
// Its three main commands are StartNetLog(), StopNetLog() and GetState(). These
// are the only functions that may cause NetLogFileWriter to change state.
// Also, NetLogFileWriter is lazily initialized. A portion of the initialization
// needs to run on the |file_task_runner_|.
//
// This class is created and destroyed on the UI thread, and all public entry
// points are to be called on the UI thread. Internally, the class may run some
// code on the |file_task_runner_| and |net_task_runner_|.
class NetLogFileWriter {
 public:
  // The three main commands StartNetLog(), StopNetLog(), and GetState() and the
  // getter GetFilePathToCompletedLog() all accept a callback param which is
  // used to notify the caller of the results of that function. For all these
  // commands, the callback will always be executed even if the command ends up
  // being a no-op or if some failure occurs.

  using StateCallback =
      base::Callback<void(std::unique_ptr<base::DictionaryValue>)>;

  using FilePathCallback = base::Callback<void(const base::FilePath&)>;

  using DirectoryGetter = base::Callback<bool(base::FilePath*)>;

  ~NetLogFileWriter();

  // Starts collecting NetLog data into the file at |log_path|. If |log_path| is
  // empty, the default log path is used. It is a no-op if NetLogFileWriter is
  // already collecting data into a file, and |capture_mode| is ignored.
  // TODO(mmenke): That's rather weird behavior, think about improving it.
  //
  // If NetLogFileWriter is not initialized, StartNetLog() will trigger
  // initialization.
  //
  // |state_callback| will be executed at the end of StartNetLog()
  // asynchronously. If StartNetLog() is called while initialization is already
  // in progress, |state_callback| will be called after the ongoing
  // initialization finishes.
  void StartNetLog(const base::FilePath& log_path,
                   net::NetLogCaptureMode capture_mode,
                   const StateCallback& state_callback);

  // Stops collecting NetLog data into the file. It is a no-op if
  // NetLogFileWriter is currently not logging.
  //
  // |polled_data| is a JSON dictionary that will be appended to the end of the
  // log; it's for adding additional info to the log that aren't events.
  // If |context_getter| is not null, then  StopNetLog() will automatically
  // append net info (from net::GetNetInfo() retrieved using |context_getter|)
  // to |polled_data|.
  //
  // |state_callback| will be executed at the end of StopNetLog()
  // asynchronously, explicitly after the log file is complete.
  void StopNetLog(std::unique_ptr<base::DictionaryValue> polled_data,
                  scoped_refptr<net::URLRequestContextGetter> context_getter,
                  const StateCallback& state_callback);

  // Creates a Value summary of the state of the NetLogFileWriter and calls
  // |state_callback| asynchronously with that Value as the param.
  //
  // If NetLogFileWriter is not initialized, GetState() will trigger
  // initialization, and |state_callback| will be called after initialization
  // finishes.
  void GetState(const StateCallback& state_callback);

  // Gets the log filepath. |path_callback| will be used to notify the caller
  // when the filepath is retrieved. |path_callback| will be executed with an
  // empty filepath if any of the following occurs:
  // (1) The NetLogFileWriter is not initialized.
  // (2) The log file does not exist.
  // (3) The NetLogFileWriter is currently logging.
  // (4) The log file's permissions could not be set to all.
  //
  // |path_callback| will be executed at the end of GetFilePathToCompletedLog()
  // asynchronously.
  void GetFilePathToCompletedLog(const FilePathCallback& path_callback) const;

  // Sets the task runners used by NetLogFileWriter for doing file I/O and
  // network I/O respectively. This must be called prior to using the
  // NetLogFileWriter. The task runners must not be changed once set. However,
  // calling this function again with the same parameters is OK.
  void SetTaskRunners(
      scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> net_task_runner);

  // Converts to/from the string representation of a capture mode used by
  // net_export.js.
  static std::string CaptureModeToString(net::NetLogCaptureMode capture_mode);
  static net::NetLogCaptureMode CaptureModeFromString(
      const std::string& capture_mode_string);

  // Overrides the getter used to retrieve the default log base directory during
  // initialization. Should only be used by unit tests.
  void SetDefaultLogBaseDirectoryGetterForTest(const DirectoryGetter& getter);

 protected:
  // Constructs a NetLogFileWriter. Only one instance is created in browser
  // process.
  NetLogFileWriter(ChromeNetLog* chrome_net_log,
                   const base::CommandLine::StringType& command_line_string,
                   const std::string& channel_string);

 private:
  friend class ChromeNetLog;
  friend class NetLogFileWriterTest;

  // The possible logging states of NetLogFileWriter.
  enum State {
    STATE_UNINITIALIZED,
    // Currently in the process of initializing.
    STATE_INITIALIZING,
    // Not currently logging to file.
    STATE_NOT_LOGGING,
    // Currently logging to file.
    STATE_LOGGING,
    // Currently in the process of stopping the log.
    STATE_STOPPING_LOG,
  };

  // Struct used to store the results of SetUpDefaultLogPath() which will be
  // passed to InitStateThenCallback().
  struct DefaultLogPathResults {
    bool default_log_path_success;
    base::FilePath default_log_path;
    bool log_exists;
  };

  // Helper function used by StartNetLog() and GetState(). If NetLogFileWriter
  // is uninitialized, this function will attempt initialization and then
  // run its callbacks.
  //
  // |after_successful_init_callback| is only called after a successful
  // initialization has completed (triggered by this call or a previous call).
  // This callback is used to do actual work outside of initialization.
  //
  // |state_callback| is called at the end asynchronously no matter what. It's
  // used to notify the caller of the state of NetLogFileWriter after
  // everything's done. If EnsureInitThenRun() is called while a previously-
  // triggered initialization is still ongoing, |state_callback| will wait
  // until after the ongoing intialization finishes before running.
  void EnsureInitThenRun(const base::Closure& after_successful_init_callback,
                         const StateCallback& state_callback);

  // Contains file-related initialization tasks. Will run on the file task
  // runner.
  static DefaultLogPathResults SetUpDefaultLogPath(
      const DirectoryGetter& default_log_base_directory_getter);

  // Will initialize NetLogFileWriter's state variables using the result of
  // SetUpDefaultLogPath().
  //
  // |after_successful_init_callback| is only executed if
  // |set_up_default_log_path_results| indicates SetUpDefaultLogPath()
  // succeeded.
  //
  // |state_callback| is executed at the end synchronously in all cases.
  void SetStateAfterSetUpDefaultLogPathThenRun(
      const base::Closure& after_successful_init_callback,
      const StateCallback& state_callback,
      const DefaultLogPathResults& set_up_default_log_path_results);

  // Gets the state, then runs |state_callback| synchronously.
  void RunStateCallback(const StateCallback& state_callback) const;

  // Asychronously calls RunStateCallback().
  void RunStateCallbackAsync(const StateCallback& state_callback);

  // Called internally by StartNetLog(). Does the actual work needed by
  // StartNetLog() outside of initialization and running the state callback.
  void StartNetLogAfterInitialized(const base::FilePath& log_path,
                                   net::NetLogCaptureMode capture_mode);

  // Called internally by StopNetLog(). Does the actual work needed by
  // StopNetLog() outside of retrieving the net info.
  void StopNetLogAfterAddNetInfo(
      const StateCallback& state_callback,
      std::unique_ptr<base::DictionaryValue> polled_data);

  // Contains tasks to be done after |write_to_file_observer_| has completely
  // stopped writing.
  void ResetObserverThenSetStateNotLogging(const StateCallback& state_callback);

  std::unique_ptr<base::DictionaryValue> GetState() const;

  // All members are accessed solely from the main thread (the thread that
  // |thread_checker_| is bound to).

  base::ThreadChecker thread_checker_;

  // Task runners for file-specific and net-specific tasks that must run on a
  // file or net thread.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> net_task_runner_;

  State state_;  // Current logging state of NetLogFileWriter.

  bool log_exists_;  // Whether or not the log exists on disk.
  bool log_capture_mode_known_;
  net::NetLogCaptureMode log_capture_mode_;

  base::FilePath log_path_;  // base::FilePath to the NetLog file.

  // |write_to_file_observer_| watches the NetLog event stream, and
  // sends all entries to the file created in StartNetLog().
  std::unique_ptr<net::FileNetLogObserver> write_to_file_observer_;

  // The |chrome_net_log_| is owned by the browser process, cached here to avoid
  // using global (g_browser_process).
  ChromeNetLog* chrome_net_log_;

  const base::CommandLine::StringType command_line_string_;
  const std::string channel_string_;

  // Used by unit tests to override the default log base directory retrieved
  // during initialization. This getter is initialized to base::GetTempDir().
  DirectoryGetter default_log_base_directory_getter_;

  base::WeakPtrFactory<NetLogFileWriter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetLogFileWriter);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_NET_LOG_FILE_WRITER_H_
