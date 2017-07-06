// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NET_LOG_NET_EXPORT_FILE_WRITER_H_
#define COMPONENTS_NET_LOG_NET_EXPORT_FILE_WRITER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "net/log/net_log_capture_mode.h"

namespace base {
class DictionaryValue;
class SingleThreadTaskRunner;
}  // namespace base

namespace net {
class FileNetLogObserver;
class URLRequestContextGetter;
}  // namespace net

namespace net_log {

class ChromeNetLog;

// NetExportFileWriter is used exclusively as a support class for net-export.
// It's a singleton that acts as the interface to all NetExportMessageHandlers
// which can tell it to start or stop logging in response to user actions from
// net-export UIs. Because it's a singleton, the logging state can be shared
// between multiple instances of the net-export UI. Internally, it manages an
// instance of net::FileNetLogObserver and handles the attaching/detaching of it
// to the ChromeNetLog. This class is used by the iOS and non-iOS
// implementations of net-export.
//
// NetExportFileWriter maintains the current logging state using the members
// |state_|, |log_exists_|, |log_capture_mode_known_|, |log_capture_mode_|.
// Its three main commands are Initialize(), StartNetLog(), and StopNetLog().
// These are the only functions that may cause NetExportFileWriter to change
// state. Initialize() must be called before NetExportFileWriter can process any
// other commands. A portion of the initialization needs to run on the
// |file_task_runner_|.
//
// This class is created and destroyed on the UI thread, and all public entry
// points are to be called on the UI thread. Internally, the class may run some
// code on the |file_task_runner_| and |net_task_runner_|.
class NetExportFileWriter {
 public:
  // The observer interface to be implemented by code that wishes to be notified
  // of NetExportFileWriter's state changes.
  class StateObserver {
   public:
    virtual void OnNewState(const base::DictionaryValue& state) = 0;
  };

  // Struct used to store the results of setting up the default log directory
  // and log path.
  struct DefaultLogPathResults {
    bool default_log_path_success;
    base::FilePath default_log_path;
    bool log_exists;
  };

  using FilePathCallback = base::Callback<void(const base::FilePath&)>;
  using DirectoryGetter = base::Callback<bool(base::FilePath*)>;
  using URLRequestContextGetterList =
      std::vector<scoped_refptr<net::URLRequestContextGetter>>;

  ~NetExportFileWriter();

  // Attaches a StateObserver. |observer| will be notified of state changes to
  // NetExportFileWriter. State changes may occur in response to Initiailze(),
  // StartNetLog(), or StopNetLog(). StateObserver::OnNewState() will be called
  // asynchronously relative to the command that caused the state change.
  // |observer| must remain alive until RemoveObserver() is called.
  void AddObserver(StateObserver* observer);

  // Detaches a StateObserver.
  void RemoveObserver(StateObserver* observer);

  // Initializes NetExportFileWriter if not initialized.
  //
  // Also sets the task runners used by NetExportFileWriter for doing file I/O
  // and network I/O respectively. The task runners must not be changed once
  // set. However, calling this function again with the same parameters is OK.
  void Initialize(scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
                  scoped_refptr<base::SingleThreadTaskRunner> net_task_runner);

  // Starts collecting NetLog data into the file at |log_path|. If |log_path| is
  // empty, the default log path is used. If NetExportFileWriter is already
  // logging, this is a no-op and |capture_mode| is ignored.
  //
  // |context_getters| is an optional list of URLRequestContextGetters used only
  // to add log entries for ongoing events when logging starts. They are not
  // used for retrieving polled data. All the contexts must be bound to the same
  // thread.
  void StartNetLog(const base::FilePath& log_path,
                   net::NetLogCaptureMode capture_mode,
                   const base::CommandLine::StringType& command_line_string,
                   const std::string& channel_string,
                   const URLRequestContextGetterList& context_getters);

  // Stops collecting NetLog data into the file. It is a no-op if
  // NetExportFileWriter is currently not logging.
  //
  // |polled_data| is a JSON dictionary that will be appended to the end of the
  // log; it's for adding additional info to the log that aren't events.
  // If |context_getter| is not null, then  StopNetLog() will automatically
  // append net info (from net::GetNetInfo() retrieved using |context_getter|)
  // to |polled_data|.
  // Note that StopNetLog() accepts (optionally) only one context getter for
  // retrieving net polled data as opposed to StartNetLog() which accepts zero
  // or more context getters for retrieving ongoing net events.
  void StopNetLog(std::unique_ptr<base::DictionaryValue> polled_data,
                  scoped_refptr<net::URLRequestContextGetter> context_getter);

  // Creates a DictionaryValue summary of the state of the NetExportFileWriter
  std::unique_ptr<base::DictionaryValue> GetState() const;

  // Gets the log filepath. |path_callback| will be used to notify the caller
  // when the filepath is retrieved. |path_callback| will be executed with an
  // empty filepath if any of the following occurs:
  // (1) The NetExportFileWriter is not initialized.
  // (2) The log file does not exist.
  // (3) The NetExportFileWriter is currently logging.
  // (4) The log file's permissions could not be set to all.
  //
  // |path_callback| will be executed at the end of GetFilePathToCompletedLog()
  // asynchronously.
  void GetFilePathToCompletedLog(const FilePathCallback& path_callback) const;

  // Converts to/from the string representation of a capture mode used by
  // net_export.js.
  static std::string CaptureModeToString(net::NetLogCaptureMode capture_mode);
  static net::NetLogCaptureMode CaptureModeFromString(
      const std::string& capture_mode_string);

  // Overrides the getter used to retrieve the default log base directory during
  // initialization. Should only be used by unit tests.
  void SetDefaultLogBaseDirectoryGetterForTest(const DirectoryGetter& getter);

 protected:
  // Constructs a NetExportFileWriter. Only one instance is created in browser
  // process.
  explicit NetExportFileWriter(ChromeNetLog* chrome_net_log);

 private:
  friend class ChromeNetLog;
  friend class NetExportFileWriterTest;

  // The possible logging states of NetExportFileWriter.
  enum State {
    STATE_UNINITIALIZED,
    // Currently in the process of initializing.
    STATE_INITIALIZING,
    // Not currently logging to file.
    STATE_NOT_LOGGING,
    // Currently in the process of starting the log.
    STATE_STARTING_LOG,
    // Currently logging to file.
    STATE_LOGGING,
    // Currently in the process of stopping the log.
    STATE_STOPPING_LOG,
  };

  void NotifyStateObservers();

  // Posts NotifyStateObservers() to the current thread.
  void NotifyStateObserversAsync();

  // Called internally by Initialize(). Will initialize NetExportFileWriter's
  // state variables after the default log directory is set up and the default
  // log path is determined on the |file_task_runner_|.
  void SetStateAfterSetUpDefaultLogPath(
      const DefaultLogPathResults& set_up_default_log_path_results);

  // Called internally by StartNetLog(). Contains tasks to be done to start
  // logging after net log entries for ongoing events are added to the log from
  // the |net_task_runner_|.
  void StartNetLogAfterCreateEntriesForActiveObjects(
      net::NetLogCaptureMode capture_mode);

  // Called internally by StopNetLog(). Contains tasks to be done to stop
  // logging after net-thread polled data is retrieved on the
  // |net_task_runner_|.
  void StopNetLogAfterAddNetInfo(
      std::unique_ptr<base::DictionaryValue> polled_data);

  // Contains tasks to be done after |file_net_log_observer_| has completely
  // stopped writing.
  void ResetObserverThenSetStateNotLogging();

  // All members are accessed solely from the main thread (the thread that
  // |thread_checker_| is bound to).

  base::ThreadChecker thread_checker_;

  // Task runners for file-specific and net-specific tasks that must run on a
  // file or net thread.
  scoped_refptr<base::SingleThreadTaskRunner> file_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> net_task_runner_;

  State state_;  // Current logging state of NetExportFileWriter.

  bool log_exists_;  // Whether or not a log file exists on disk.
  bool log_capture_mode_known_;
  net::NetLogCaptureMode log_capture_mode_;

  base::FilePath log_path_;  // base::FilePath to the NetLog file.

  // |file_net_log_observer_| watches the NetLog event stream, and
  // sends all entries to the file created in StartNetLog().
  std::unique_ptr<net::FileNetLogObserver> file_net_log_observer_;

  // The |chrome_net_log_| is owned by the browser process, cached here to avoid
  // using global (g_browser_process).
  ChromeNetLog* chrome_net_log_;

  // List of StateObservers to notify on state changes.
  base::ObserverList<StateObserver, true> state_observer_list_;

  // Used by unit tests to override the default log base directory retrieved
  // during initialization. This getter is initialized to base::GetTempDir().
  DirectoryGetter default_log_base_dir_getter_;

  base::WeakPtrFactory<NetExportFileWriter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetExportFileWriter);
};

}  // namespace net_log

#endif  // COMPONENTS_NET_LOG_NET_LOG_FILE_WRITER_H_
