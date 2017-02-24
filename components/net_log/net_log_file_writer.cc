// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_log_file_writer.h"

#include <set>
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

namespace net {
class URLRequestContext;
}

namespace net_log {

namespace {

// Path of logs relative to default temporary directory given by
// base::GetTempDir(). Must be kept in sync with
// chrome/android/java/res/xml/file_paths.xml. Only used if not saving log file
// to a custom path.
const base::FilePath::CharType kLogRelativePath[] =
    FILE_PATH_LITERAL("net-export/chrome-net-export-log.json");

// Old path used by net-export. Used to delete old files.
// TODO(mmenke): Should remove at some point. Added in M46.
const base::FilePath::CharType kOldLogRelativePath[] =
    FILE_PATH_LITERAL("chrome-net-export-log.json");

// Contains file-related initialization tasks for NetLogFileWriter.
NetLogFileWriter::DefaultLogPathResults SetUpDefaultLogPath(
    const NetLogFileWriter::DirectoryGetter& default_log_base_dir_getter) {
  NetLogFileWriter::DefaultLogPathResults results;
  results.default_log_path_success = false;
  results.log_exists = false;

  base::FilePath default_base_dir;
  if (!default_log_base_dir_getter.Run(&default_base_dir))
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

// Generates net log entries for ongoing events from |context_getters| and
// adds them to |observer|.
void CreateNetLogEntriesForActiveObjects(
    const NetLogFileWriter::URLRequestContextGetterList& context_getters,
    net::NetLog::ThreadSafeObserver* observer) {
  std::set<net::URLRequestContext*> contexts;
  for (const auto& getter : context_getters) {
    DCHECK(getter->GetNetworkTaskRunner()->BelongsToCurrentThread());
    contexts.insert(getter->GetURLRequestContext());
  }
  net::CreateNetLogEntriesForActiveObjects(contexts, observer);
}

// Adds net info from net::GetNetInfo() to |polled_data|.
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

}  // namespace

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
      default_log_base_dir_getter_(base::Bind(&base::GetTempDir)),
      weak_ptr_factory_(this) {}

NetLogFileWriter::~NetLogFileWriter() {
  if (file_net_log_observer_)
    file_net_log_observer_->StopObserving(nullptr, base::Bind([] {}));
}

void NetLogFileWriter::AddObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.AddObserver(observer);
}

void NetLogFileWriter::RemoveObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.RemoveObserver(observer);
}

void NetLogFileWriter::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> net_task_runner) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(file_task_runner);
  DCHECK(net_task_runner);

  if (file_task_runner_)
    DCHECK_EQ(file_task_runner_, file_task_runner);
  file_task_runner_ = file_task_runner;
  if (net_task_runner_)
    DCHECK_EQ(net_task_runner_, net_task_runner);
  net_task_runner_ = net_task_runner;

  if (state_ != STATE_UNINITIALIZED)
    return;

  state_ = STATE_INITIALIZING;

  NotifyStateObserversAsync();

  base::PostTaskAndReplyWithResult(
      file_task_runner_.get(), FROM_HERE,
      base::Bind(&SetUpDefaultLogPath, default_log_base_dir_getter_),
      base::Bind(&NetLogFileWriter::SetStateAfterSetUpDefaultLogPath,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetLogFileWriter::StartNetLog(
    const base::FilePath& log_path,
    net::NetLogCaptureMode capture_mode,
    const URLRequestContextGetterList& context_getters) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(file_task_runner_);

  if (state_ != STATE_NOT_LOGGING)
    return;

  if (!log_path.empty())
    log_path_ = log_path;

  DCHECK(!log_path_.empty());

  state_ = STATE_STARTING_LOG;

  NotifyStateObserversAsync();

  std::unique_ptr<base::Value> constants(
      ChromeNetLog::GetConstants(command_line_string_, channel_string_));
  // Instantiate a FileNetLogObserver in unbounded mode.
  file_net_log_observer_ = net::FileNetLogObserver::CreateUnbounded(
      file_task_runner_, log_path_, std::move(constants));

  net_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CreateNetLogEntriesForActiveObjects, context_getters,
                 base::Unretained(file_net_log_observer_.get())),
      base::Bind(
          &NetLogFileWriter::StartNetLogAfterCreateEntriesForActiveObjects,
          weak_ptr_factory_.GetWeakPtr(), capture_mode));
}

void NetLogFileWriter::StartNetLogAfterCreateEntriesForActiveObjects(
    net::NetLogCaptureMode capture_mode) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_STARTING_LOG, state_);

  state_ = STATE_LOGGING;
  log_exists_ = true;
  log_capture_mode_known_ = true;
  log_capture_mode_ = capture_mode;

  NotifyStateObservers();

  file_net_log_observer_->StartObserving(chrome_net_log_, capture_mode);
}

void NetLogFileWriter::StopNetLog(
    std::unique_ptr<base::DictionaryValue> polled_data,
    scoped_refptr<net::URLRequestContextGetter> context_getter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(net_task_runner_);

  if (state_ != STATE_LOGGING)
    return;

  state_ = STATE_STOPPING_LOG;

  NotifyStateObserversAsync();

  if (context_getter) {
    base::PostTaskAndReplyWithResult(
        net_task_runner_.get(), FROM_HERE,
        base::Bind(&AddNetInfo, context_getter, base::Passed(&polled_data)),
        base::Bind(&NetLogFileWriter::StopNetLogAfterAddNetInfo,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    StopNetLogAfterAddNetInfo(std::move(polled_data));
  }
}

std::unique_ptr<base::DictionaryValue> NetLogFileWriter::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dict = base::MakeUnique<base::DictionaryValue>();

#ifndef NDEBUG
  dict->SetString("file", log_path_.LossyDisplayName());
#endif  // NDEBUG

  base::StringPiece state_string;
  switch (state_) {
    case STATE_UNINITIALIZED:
      state_string = "UNINITIALIZED";
      break;
    case STATE_INITIALIZING:
      state_string = "INITIALIZING";
      break;
    case STATE_NOT_LOGGING:
      state_string = "NOT_LOGGING";
      break;
    case STATE_STARTING_LOG:
      state_string = "STARTING_LOG";
      break;
    case STATE_LOGGING:
      state_string = "LOGGING";
      break;
    case STATE_STOPPING_LOG:
      state_string = "STOPPING_LOG";
      break;
  }
  dict->SetString("state", state_string);

  dict->SetBoolean("logExists", log_exists_);
  dict->SetBoolean("logCaptureModeKnown", log_capture_mode_known_);
  dict->SetString("captureMode", CaptureModeToString(log_capture_mode_));

  return dict;
}

void NetLogFileWriter::GetFilePathToCompletedLog(
    const FilePathCallback& path_callback) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!(log_exists_ && state_ == STATE_NOT_LOGGING)) {
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
  default_log_base_dir_getter_ = getter;
}

void NetLogFileWriter::NotifyStateObservers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::DictionaryValue> state = GetState();
  for (StateObserver& observer : state_observer_list_) {
    observer.OnNewState(*state);
  }
}

void NetLogFileWriter::NotifyStateObserversAsync() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&NetLogFileWriter::NotifyStateObservers,
                            weak_ptr_factory_.GetWeakPtr()));
}

void NetLogFileWriter::SetStateAfterSetUpDefaultLogPath(
    const DefaultLogPathResults& set_up_default_log_path_results) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_INITIALIZING, state_);

  if (set_up_default_log_path_results.default_log_path_success) {
    state_ = STATE_NOT_LOGGING;
    log_path_ = set_up_default_log_path_results.default_log_path;
    log_exists_ = set_up_default_log_path_results.log_exists;
    DCHECK(!log_capture_mode_known_);
  } else {
    state_ = STATE_UNINITIALIZED;
  }
  NotifyStateObservers();
}

void NetLogFileWriter::StopNetLogAfterAddNetInfo(
    std::unique_ptr<base::DictionaryValue> polled_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_STOPPING_LOG, state_);

  file_net_log_observer_->StopObserving(
      std::move(polled_data),
      base::Bind(&NetLogFileWriter::ResetObserverThenSetStateNotLogging,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetLogFileWriter::ResetObserverThenSetStateNotLogging() {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_net_log_observer_.reset();
  state_ = STATE_NOT_LOGGING;

  NotifyStateObservers();
}

}  // namespace net_log
