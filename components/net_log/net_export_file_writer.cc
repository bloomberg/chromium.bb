// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/net_log/net_export_file_writer.h"

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

// Contains file-related initialization tasks for NetExportFileWriter.
NetExportFileWriter::DefaultLogPathResults SetUpDefaultLogPath(
    const NetExportFileWriter::DirectoryGetter& default_log_base_dir_getter) {
  NetExportFileWriter::DefaultLogPathResults results;
  results.default_log_path_success = false;
  results.log_exists = false;

  base::FilePath default_base_dir;
  if (!default_log_base_dir_getter.Run(&default_base_dir))
    return results;

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
    const NetExportFileWriter::URLRequestContextGetterList& context_getters,
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

const size_t NetExportFileWriter::kNoLimit = net::FileNetLogObserver::kNoLimit;

NetExportFileWriter::NetExportFileWriter(ChromeNetLog* chrome_net_log)
    : state_(STATE_UNINITIALIZED),
      log_exists_(false),
      log_capture_mode_known_(false),
      log_capture_mode_(net::NetLogCaptureMode::Default()),
      chrome_net_log_(chrome_net_log),
      default_log_base_dir_getter_(base::Bind(&base::GetTempDir)),
      weak_ptr_factory_(this) {}

NetExportFileWriter::~NetExportFileWriter() {
  if (file_net_log_observer_)
    file_net_log_observer_->StopObserving(nullptr, base::Closure());
}

void NetExportFileWriter::AddObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.AddObserver(observer);
}

void NetExportFileWriter::RemoveObserver(StateObserver* observer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_observer_list_.RemoveObserver(observer);
}

void NetExportFileWriter::Initialize(
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
      base::Bind(&NetExportFileWriter::SetStateAfterSetUpDefaultLogPath,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetExportFileWriter::StartNetLog(
    const base::FilePath& log_path,
    net::NetLogCaptureMode capture_mode,
    size_t max_file_size,
    const base::CommandLine::StringType& command_line_string,
    const std::string& channel_string,
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
      ChromeNetLog::GetConstants(command_line_string, channel_string));

  file_net_log_observer_ = net::FileNetLogObserver::CreateBounded(
      log_path_, max_file_size, std::move(constants));

  net_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CreateNetLogEntriesForActiveObjects, context_getters,
                 base::Unretained(file_net_log_observer_.get())),
      base::Bind(
          &NetExportFileWriter::StartNetLogAfterCreateEntriesForActiveObjects,
          weak_ptr_factory_.GetWeakPtr(), capture_mode));
}

void NetExportFileWriter::StartNetLogAfterCreateEntriesForActiveObjects(
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

void NetExportFileWriter::StopNetLog(
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
        base::Bind(&NetExportFileWriter::StopNetLogAfterAddNetInfo,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    StopNetLogAfterAddNetInfo(std::move(polled_data));
  }
}

std::unique_ptr<base::DictionaryValue> NetExportFileWriter::GetState() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  auto dict = base::MakeUnique<base::DictionaryValue>();

  dict->SetString("file", log_path_.LossyDisplayName());

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

void NetExportFileWriter::GetFilePathToCompletedLog(
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

std::string NetExportFileWriter::CaptureModeToString(
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

net::NetLogCaptureMode NetExportFileWriter::CaptureModeFromString(
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

void NetExportFileWriter::SetDefaultLogBaseDirectoryGetterForTest(
    const DirectoryGetter& getter) {
  default_log_base_dir_getter_ = getter;
}

void NetExportFileWriter::NotifyStateObservers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::unique_ptr<base::DictionaryValue> state = GetState();
  for (StateObserver& observer : state_observer_list_) {
    observer.OnNewState(*state);
  }
}

void NetExportFileWriter::NotifyStateObserversAsync() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&NetExportFileWriter::NotifyStateObservers,
                            weak_ptr_factory_.GetWeakPtr()));
}

void NetExportFileWriter::SetStateAfterSetUpDefaultLogPath(
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

void NetExportFileWriter::StopNetLogAfterAddNetInfo(
    std::unique_ptr<base::DictionaryValue> polled_data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(STATE_STOPPING_LOG, state_);

  file_net_log_observer_->StopObserving(
      std::move(polled_data),
      base::Bind(&NetExportFileWriter::ResetObserverThenSetStateNotLogging,
                 weak_ptr_factory_.GetWeakPtr()));
}

void NetExportFileWriter::ResetObserverThenSetStateNotLogging() {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_net_log_observer_.reset();
  state_ = STATE_NOT_LOGGING;

  NotifyStateObservers();
}

}  // namespace net_log
