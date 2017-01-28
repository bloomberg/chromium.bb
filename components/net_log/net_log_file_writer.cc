// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_file_writer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/net_log/chrome_net_log.h"
#include "net/log/file_net_log_observer.h"
#include "net/log/net_log_util.h"
#include "net/url_request/url_request_context_getter.h"

namespace net_log {

// Path of logs relative to default temporary directory given by
// base::GetTempDir(). Must be kept in sync with
// chrome/android/java/res/xml/file_paths.xml. Only used if not saving log file
// to a custom path.
base::FilePath::CharType kLogRelativePath[] =
    FILE_PATH_LITERAL("net-export/chrome-net-export-log.json");

// Old path used by net-export. Used to delete old files.
// TODO(mmenke): Should remove at some point. Added in M46.
base::FilePath::CharType kOldLogRelativePath[] =
    FILE_PATH_LITERAL("chrome-net-export-log.json");

// Adds net info from net::GetNetInfo() to |polled_data|. Must run on the
// |net_task_runner_| of NetLogFileWriter.
std::unique_ptr<base::DictionaryValue> AddNetInfo(
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    std::unique_ptr<base::DictionaryValue> polled_data) {
  DCHECK(context_getter);
  std::unique_ptr<base::DictionaryValue> net_info = net::GetNetInfo(
      context_getter->GetURLRequestContext(), net::NET_INFO_ALL_SOURCES);
  if (polled_data)
    net_info->MergeDictionary(polled_data.get());
  return net_info;
}

// If running on a POSIX OS, this will attempt to set all the permission flags
// of the file at |path| to 1. Will return |path| on success and the empty path
// on failure.
base::FilePath GetPathWithAllPermissions(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::FilePath();
#if defined(OS_POSIX)
  return base::SetPosixFilePermissions(path, base::FILE_PERMISSION_MASK)
             ? path
             : base::FilePath();
#else
  return path;
#endif
}

NetLogFileWriter::NetLogFileWriter(
    ChromeNetLog* chrome_net_log,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string)
    : state_(STATE_UNINITIALIZED),
      log_exists_(false),
      log_capture_mode_known_(false),
      log_capture_mode_(net::NetLogCaptureMode::Default()),
      chrome_net_log_(chrome_net_log),
      command_line_string_(command_line_string),
      channel_string_(channel_string),
      default_log_base_directory_getter_(base::Bind(&base::GetTempDir)),
      weak_ptr_factory_(this) {}

NetLogFileWriter::~NetLogFileWriter() {
  if (write_to_file_observer_)
    write_to_file_observer_->StopObserving(nullptr, base::Bind([] {}));
}

void NetLogFileWriter::StartNetLog(const base::FilePath& log_path,
                                   net::NetLogCaptureMode capture_mode,
                                   const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  EnsureInitThenRun(
      base::Bind(&NetLogFileWriter::StartNetLogAfterInitialized,
                 weak_ptr_factory_.GetWeakPtr(), log_path, capture_mode),
      state_callback);
}

void NetLogFileWriter::StopNetLog(
    std::unique_ptr<base::DictionaryValue> polled_data,
    scoped_refptr<net::URLRequestContextGetter> context_getter,
    const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(net_task_runner_);
  if (state_ == STATE_LOGGING) {
    // Stopping the log requires first grabbing the net info on the net thread.
    // Before posting that task to the net thread, change the state to
    // STATE_STOP_LOGGING so that if the NetLogFileWriter receives a command
    // while the net info is being retrieved on the net thread, the state can be
    // checked and the command can be ignored. It's the responsibility of the
    // commands (StartNetLog(), StopNetLog(), GetState()) to check the state
    // before performing their actions.
    state_ = STATE_STOPPING_LOG;

    // StopLogging() will always execute its state callback asynchronously,
    // which means |state_callback| will always be executed asynchronously
    // relative to the StopNetLog() call regardless of how StopLogging() is
    // called here.

    if (context_getter) {
      base::PostTaskAndReplyWithResult(
          net_task_runner_.get(), FROM_HERE,
          base::Bind(&AddNetInfo, context_getter, base::Passed(&polled_data)),
          base::Bind(&NetLogFileWriter::StopNetLogAfterAddNetInfo,
                     weak_ptr_factory_.GetWeakPtr(), state_callback));
    } else {
      StopNetLogAfterAddNetInfo(state_callback, std::move(polled_data));
    }
  } else {
    // No-op; just run |state_callback| asynchronously.
    RunStateCallbackAsync(state_callback);
  }
}

void NetLogFileWriter::GetState(const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  EnsureInitThenRun(base::Bind([] {}), state_callback);
}

void NetLogFileWriter::GetFilePathToCompletedLog(
    const FilePathCallback& path_callback) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!log_exists_ || state_ == STATE_LOGGING) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(path_callback, base::FilePath()));
    return;
  }

  DCHECK(file_task_runner_);
  DCHECK(!log_path_.empty());

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&GetPathWithAllPermissions, log_path_), path_callback);
}

void NetLogFileWriter::SetTaskRunners(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> net_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (file_task_runner_)
    DCHECK_EQ(file_task_runner, file_task_runner_);
  file_task_runner_ = file_task_runner;

  if (net_task_runner_)
    DCHECK_EQ(net_task_runner, net_task_runner_);
  net_task_runner_ = net_task_runner;
}

std::string NetLogFileWriter::CaptureModeToString(
    net::NetLogCaptureMode capture_mode) {
  if (capture_mode == net::NetLogCaptureMode::Default()) {
    return "STRIP_PRIVATE_DATA";
  } else if (capture_mode ==
             net::NetLogCaptureMode::IncludeCookiesAndCredentials()) {
    return "NORMAL";
  } else if (capture_mode == net::NetLogCaptureMode::IncludeSocketBytes()) {
    return "LOG_BYTES";
  } else {
    NOTREACHED();
    return "STRIP_PRIVATE_DATA";
  }
}

net::NetLogCaptureMode NetLogFileWriter::CaptureModeFromString(
    const std::string& capture_mode_string) {
  if (capture_mode_string == "STRIP_PRIVATE_DATA") {
    return net::NetLogCaptureMode::Default();
  } else if (capture_mode_string == "NORMAL") {
    return net::NetLogCaptureMode::IncludeCookiesAndCredentials();
  } else if (capture_mode_string == "LOG_BYTES") {
    return net::NetLogCaptureMode::IncludeSocketBytes();
  } else {
    NOTREACHED();
    return net::NetLogCaptureMode::Default();
  }
}

void NetLogFileWriter::SetDefaultLogBaseDirectoryGetterForTest(
    const DirectoryGetter& getter) {
  default_log_base_directory_getter_ = getter;
}

void NetLogFileWriter::EnsureInitThenRun(
    const base::Closure& after_successful_init_callback,
    const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (state_ == STATE_UNINITIALIZED) {
    state_ = STATE_INITIALIZING;
    // Run initialization tasks on the file thread, then the main thread. Once
    // finished, run |after_successful_init_callback| and |state_callback| on
    // the main thread.
    base::PostTaskAndReplyWithResult(
        file_task_runner_.get(), FROM_HERE,
        base::Bind(&NetLogFileWriter::SetUpDefaultLogPath,
                   default_log_base_directory_getter_),
        base::Bind(&NetLogFileWriter::SetStateAfterSetUpDefaultLogPathThenRun,
                   weak_ptr_factory_.GetWeakPtr(),
                   after_successful_init_callback, state_callback));
  } else if (state_ == STATE_INITIALIZING) {
    // If NetLogFileWriter is already in the process of initializing due to a
    // previous call to EnsureInitThenRun(), commands received by
    // NetLogFileWriter should be ignored, so only |state_callback| will be
    // executed.
    // Wait for the in-progress initialization to finish before calling
    // |state_callback|. To do this, post a dummy task to the file thread
    // (which is guaranteed to run after the tasks belonging to the
    // in-progress initialization have finished), and have that dummy task
    // post |state_callback| as a reply on the main thread.
    file_task_runner_->PostTaskAndReply(
        FROM_HERE, base::Bind([] {}),
        base::Bind(&NetLogFileWriter::RunStateCallback,
                   weak_ptr_factory_.GetWeakPtr(), state_callback));

  } else {
    // NetLogFileWriter is already fully initialized. Run
    // |after_successful_init_callback| synchronously and |state_callback|
    // asynchronously.
    after_successful_init_callback.Run();
    RunStateCallbackAsync(state_callback);
  }
}

NetLogFileWriter::DefaultLogPathResults NetLogFileWriter::SetUpDefaultLogPath(
    const DirectoryGetter& default_log_base_directory_getter) {
  DefaultLogPathResults results;
  results.default_log_path_success = false;
  results.log_exists = false;

  base::FilePath default_base_dir;
  if (!default_log_base_directory_getter.Run(&default_base_dir))
    return results;

  // Delete log file at old location, if present.
  base::DeleteFile(default_base_dir.Append(kOldLogRelativePath), false);

  results.default_log_path = default_base_dir.Append(kLogRelativePath);
  if (!base::CreateDirectoryAndGetError(results.default_log_path.DirName(),
                                        nullptr))
    return results;

  results.log_exists = base::PathExists(results.default_log_path);
  results.default_log_path_success = true;
  return results;
}

void NetLogFileWriter::SetStateAfterSetUpDefaultLogPathThenRun(
    const base::Closure& after_successful_init_callback,
    const StateCallback& state_callback,
    const DefaultLogPathResults& set_up_default_log_path_results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_INITIALIZING);

  if (set_up_default_log_path_results.default_log_path_success) {
    state_ = STATE_NOT_LOGGING;
    log_path_ = set_up_default_log_path_results.default_log_path;
    log_exists_ = set_up_default_log_path_results.log_exists;
    DCHECK(!log_capture_mode_known_);

    after_successful_init_callback.Run();
  } else {
    state_ = STATE_UNINITIALIZED;
  }

  RunStateCallback(state_callback);
}

void NetLogFileWriter::RunStateCallback(
    const StateCallback& state_callback) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_callback.Run(GetState());
}

void NetLogFileWriter::RunStateCallbackAsync(
    const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&NetLogFileWriter::RunStateCallback,
                            weak_ptr_factory_.GetWeakPtr(), state_callback));
}

void NetLogFileWriter::StartNetLogAfterInitialized(
    const base::FilePath& log_path,
    net::NetLogCaptureMode capture_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(state_ != STATE_UNINITIALIZED && state_ != STATE_INITIALIZING);
  DCHECK(file_task_runner_);

  if (state_ == STATE_NOT_LOGGING) {
    if (!log_path.empty())
      log_path_ = log_path;

    DCHECK(!log_path_.empty());

    state_ = STATE_LOGGING;
    log_exists_ = true;
    log_capture_mode_known_ = true;
    log_capture_mode_ = capture_mode;

    std::unique_ptr<base::Value> constants(
        ChromeNetLog::GetConstants(command_line_string_, channel_string_));
    write_to_file_observer_ =
        base::MakeUnique<net::FileNetLogObserver>(file_task_runner_);
    write_to_file_observer_->StartObservingUnbounded(
        chrome_net_log_, capture_mode, log_path_, std::move(constants),
        nullptr);
  }
}

void NetLogFileWriter::StopNetLogAfterAddNetInfo(
    const StateCallback& state_callback,
    std::unique_ptr<base::DictionaryValue> polled_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, STATE_STOPPING_LOG);

  write_to_file_observer_->StopObserving(
      std::move(polled_data),
      base::Bind(&NetLogFileWriter::ResetObserverThenSetStateNotLogging,
                 weak_ptr_factory_.GetWeakPtr(), state_callback));
}

void NetLogFileWriter::ResetObserverThenSetStateNotLogging(
    const StateCallback& state_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  write_to_file_observer_.reset();
  state_ = STATE_NOT_LOGGING;

  RunStateCallback(state_callback);
}

std::unique_ptr<base::DictionaryValue> NetLogFileWriter::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dict = base::MakeUnique<base::DictionaryValue>();

#ifndef NDEBUG
  dict->SetString("file", log_path_.LossyDisplayName());
#endif  // NDEBUG

  switch (state_) {
    case STATE_UNINITIALIZED:
      dict->SetString("state", "UNINITIALIZED");
      break;
    case STATE_INITIALIZING:
      dict->SetString("state", "INITIALIZING");
      break;
    case STATE_NOT_LOGGING:
      dict->SetString("state", "NOT_LOGGING");
      break;
    case STATE_LOGGING:
      dict->SetString("state", "LOGGING");
      break;
    case STATE_STOPPING_LOG:
      dict->SetString("state", "STOPPING_LOG");
      break;
  }

  dict->SetBoolean("logExists", log_exists_);
  dict->SetBoolean("logCaptureModeKnown", log_capture_mode_known_);
  dict->SetString("captureMode", CaptureModeToString(log_capture_mode_));

  return dict;
}

}  // namespace net_log
