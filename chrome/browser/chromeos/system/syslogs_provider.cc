// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/syslogs_provider.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/string_util.h"
#include "base/task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/memory_details.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/network/network_event_log.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"
#include "dbus/dbus_statistics.h"

using content::BrowserThread;

namespace chromeos {
namespace system {

const size_t kFeedbackMaxLength = 4 * 1024;
const size_t kFeedbackMaxLineCount = 40;

namespace {

const char kSysLogsScript[] =
    "/usr/share/userfeedback/scripts/sysinfo_script_runner";
const char kBzip2Command[] =
    "/bin/bzip2";
const char kMultilineQuote[] = "\"\"\"";
const char kNewLineChars[] = "\r\n";
const char kInvalidLogEntry[] = "<invalid characters in log entry>";
const char kEmptyLogEntry[] = "<no value>";

const char kContextFeedback[] = "feedback";
const char kContextSysInfo[] = "sysinfo";
const char kContextNetwork[] = "network";

// Reads a key from the input string erasing the read values + delimiters read
// from the initial string
std::string ReadKey(std::string* data) {
  std::string key;
  size_t equal_sign = data->find("=");
  if (equal_sign == std::string::npos)
    return key;
  key = data->substr(0, equal_sign);
  data->erase(0, equal_sign);
  if (data->empty())
    return key;
  // erase the equal to sign also
  data->erase(0, 1);
  return key;
}

// Reads a value from the input string; erasing the read values from
// the initial string; detects if the value is multiline and reads
// accordingly
std::string ReadValue(std::string* data) {
  // Trim the leading spaces and tabs. In order to use a multi-line
  // value, you have to place the multi-line quote on the same line as
  // the equal sign.
  //
  // Why not use TrimWhitespace? Consider the following input:
  //
  // KEY1=
  // KEY2=VALUE
  //
  // If we use TrimWhitespace, we will incorrectly trim the new line
  // and assume that KEY1's value is "KEY2=VALUE" rather than empty.
  base::TrimString(*data, " \t", data);

  // If multiline value
  if (StartsWithASCII(*data, std::string(kMultilineQuote), false)) {
    data->erase(0, strlen(kMultilineQuote));
    size_t next_multi = data->find(kMultilineQuote);
    if (next_multi == std::string::npos) {
      // Error condition, clear data to stop further processing
      data->erase();
      return std::string();
    }
    std::string value = data->substr(0, next_multi);
    data->erase(0, next_multi + 3);
    return value;
  } else {
    // single line value
    size_t endl_pos = data->find_first_of(kNewLineChars);
    // if we don't find a new line, we just return the rest of the data
    std::string value = data->substr(0, endl_pos);
    data->erase(0, endl_pos);
    return value;
  }
}

// Returns a map of system log keys and values.
//
// Parameters:
// temp_filename: This is an out parameter that holds the name of a file in
// Reads a value from the input string; erasing the read values from
// the initial string; detects if the value is multiline and reads
// accordingly
//                /tmp that contains the system logs in a KEY=VALUE format.
//                If this parameter is NULL, system logs are not retained on
//                the filesystem after this call completes.
// context:       This is an in parameter specifying what context should be
//                passed to the syslog collection script; currently valid
//                values are "sysinfo" or "feedback"; in case of an invalid
//                value, the script will currently default to "sysinfo"

LogDictionaryType* GetSystemLogs(base::FilePath* zip_file_name,
                                 const std::string& context) {
  // Create the temp file, logs will go here
  base::FilePath temp_filename;

  if (!base::CreateTemporaryFile(&temp_filename))
    return NULL;

  std::string cmd = std::string(kSysLogsScript) + " " + context + " >> " +
      temp_filename.value();

  // Ignore the return value - if the script execution didn't work
  // stderr won't go into the output file anyway.
  if (::system(cmd.c_str()) == -1)
    LOG(WARNING) << "Command " << cmd << " failed to run";

  // Compress the logs file if requested.
  if (zip_file_name) {
    cmd = std::string(kBzip2Command) + " -c " + temp_filename.value() + " > " +
        zip_file_name->value();
    if (::system(cmd.c_str()) == -1)
      LOG(WARNING) << "Command " << cmd << " failed to run";
  }
  // Read logs from the temp file
  std::string data;
  bool read_success = base::ReadFileToString(temp_filename, &data);
  // if we were using an internal temp file, the user does not need the
  // logs to stay past the ReadFile call - delete the file
  base::DeleteFile(temp_filename, false);

  if (!read_success)
    return NULL;

  // Parse the return data into a dictionary
  LogDictionaryType* logs = new LogDictionaryType();
  while (data.length() > 0) {
    std::string key = ReadKey(&data);
    base::TrimWhitespaceASCII(key, base::TRIM_ALL, &key);
    if (!key.empty()) {
      std::string value = ReadValue(&data);
      if (base::IsStringUTF8(value)) {
        base::TrimWhitespaceASCII(value, base::TRIM_ALL, &value);
        if (value.empty())
          (*logs)[key] = kEmptyLogEntry;
        else
          (*logs)[key] = value;
      } else {
        LOG(WARNING) << "Invalid characters in system log entry: " << key;
        (*logs)[key] = kInvalidLogEntry;
      }
    } else {
      // no more keys, we're done
      break;
    }
  }

  return logs;
}

}  // namespace

class SyslogsProviderImpl : public SyslogsProvider {
 public:
  // SyslogsProvider implementation:
  virtual base::CancelableTaskTracker::TaskId RequestSyslogs(
      bool compress_logs,
      SyslogsContext context,
      const ReadCompleteCallback& callback,
      base::CancelableTaskTracker* tracker) OVERRIDE;

  static SyslogsProviderImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SyslogsProviderImpl>;

  // Reads system logs, compresses content if requested.
  // Called from blocking pool thread.
  void ReadSyslogs(
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
      bool compress_logs,
      SyslogsContext context,
      const ReadCompleteCallback& callback);

  // Loads compressed logs and writes into |zip_content|.
  void LoadCompressedLogs(const base::FilePath& zip_file,
                          std::string* zip_content);

  SyslogsProviderImpl();

  // Gets syslogs context string from the enum value.
  const char* GetSyslogsContextString(SyslogsContext context);

  // If not canceled, run callback on originating thread (the thread on which
  // ReadSyslogs was run).
  static void RunCallbackIfNotCanceled(
      const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
      base::TaskRunner* origin_runner,
      const ReadCompleteCallback& callback,
      LogDictionaryType* logs,
      std::string* zip_content);

  DISALLOW_COPY_AND_ASSIGN(SyslogsProviderImpl);
};

SyslogsProviderImpl::SyslogsProviderImpl() {
}

base::CancelableTaskTracker::TaskId SyslogsProviderImpl::RequestSyslogs(
    bool compress_logs,
    SyslogsContext context,
    const ReadCompleteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  base::CancelableTaskTracker::IsCanceledCallback is_canceled;
  base::CancelableTaskTracker::TaskId id =
      tracker->NewTrackedTaskId(&is_canceled);

  ReadCompleteCallback callback_runner =
      base::Bind(&SyslogsProviderImpl::RunCallbackIfNotCanceled,
                 is_canceled, base::MessageLoopProxy::current(), callback);

  // Schedule a task which will run the callback later when complete.
  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&SyslogsProviderImpl::ReadSyslogs, base::Unretained(this),
                 is_canceled, compress_logs, context, callback_runner));
  return id;
}

// Derived class from memoryDetails converts the results into a single string
// and adds a "mem_usage" entry to the logs, then forwards the result.
// Format of entry is (one process per line, reverse-sorted by size):
//   Tab [Title1|Title2]: 50 MB
//   Browser: 30 MB
//   Tab [Title]: 20 MB
//   Extension [Title]: 10 MB
// ...
class SyslogsMemoryHandler : public MemoryDetails {
 public:
  typedef SyslogsProvider::ReadCompleteCallback ReadCompleteCallback;

  // |logs| is modified (see comment above) and passed to |request|.
  // |zip_content| is passed to |request|.
  SyslogsMemoryHandler(const ReadCompleteCallback& callback,
                       LogDictionaryType* logs,
                       std::string* zip_content);

  virtual void OnDetailsAvailable() OVERRIDE;

 private:
  virtual ~SyslogsMemoryHandler();

  ReadCompleteCallback callback_;

  LogDictionaryType* logs_;
  std::string* zip_content_;

  DISALLOW_COPY_AND_ASSIGN(SyslogsMemoryHandler);
};

SyslogsMemoryHandler::SyslogsMemoryHandler(
    const ReadCompleteCallback& callback,
    LogDictionaryType* logs,
    std::string* zip_content)
    : callback_(callback),
      logs_(logs),
      zip_content_(zip_content) {
  DCHECK(!callback_.is_null());
}

void SyslogsMemoryHandler::OnDetailsAvailable() {
    (*logs_)["mem_usage"] = ToLogString();
    callback_.Run(logs_, zip_content_);
  }

SyslogsMemoryHandler::~SyslogsMemoryHandler() {}

// Called from blocking pool thread.
void SyslogsProviderImpl::ReadSyslogs(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    bool compress_logs,
    SyslogsContext context,
    const ReadCompleteCallback& callback) {
  DCHECK(BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  if (is_canceled.Run())
    return;

  // Create temp file.
  base::FilePath zip_file;
  if (compress_logs && !base::CreateTemporaryFile(&zip_file)) {
    LOG(ERROR) << "Cannot create temp file";
    compress_logs = false;
  }

  LogDictionaryType* logs = NULL;
  logs = GetSystemLogs(
      compress_logs ? &zip_file : NULL,
      GetSyslogsContextString(context));

  std::string* zip_content = NULL;
  if (compress_logs) {
    // Load compressed logs.
    zip_content = new std::string();
    LoadCompressedLogs(zip_file, zip_content);
    base::DeleteFile(zip_file, false);
  }

  // Include dbus statistics summary
  (*logs)["dbus"] = dbus::statistics::GetAsString(
      dbus::statistics::SHOW_INTERFACE,
      dbus::statistics::FORMAT_ALL);

  // Include recent network log events
  (*logs)["network_event_log"] = network_event_log::GetAsString(
      network_event_log::OLDEST_FIRST,
      "time,file,desc",
      network_event_log::kDefaultLogLevel,
      system::kFeedbackMaxLineCount);

  // SyslogsMemoryHandler will clean itself up.
  // SyslogsMemoryHandler::OnDetailsAvailable() will modify |logs| and call
  // request->ForwardResult(logs, zip_content).
  scoped_refptr<SyslogsMemoryHandler>
      handler(new SyslogsMemoryHandler(callback, logs, zip_content));
  // TODO(jamescook): Maybe we don't need to update histograms here?
  handler->StartFetch(MemoryDetails::UPDATE_USER_METRICS);
}

void SyslogsProviderImpl::LoadCompressedLogs(const base::FilePath& zip_file,
                                            std::string* zip_content) {
  DCHECK(zip_content);
  if (!base::ReadFileToString(zip_file, zip_content)) {
    LOG(ERROR) << "Cannot read compressed logs file from " <<
        zip_file.value().c_str();
  }
}

const char* SyslogsProviderImpl::GetSyslogsContextString(
    SyslogsContext context) {
  switch (context) {
    case(SYSLOGS_FEEDBACK):
      return kContextFeedback;
    case(SYSLOGS_SYSINFO):
      return kContextSysInfo;
    case(SYSLOGS_NETWORK):
      return kContextNetwork;
    case(SYSLOGS_DEFAULT):
      return kContextSysInfo;
    default:
      NOTREACHED();
      return "";
  }
}

// static
void SyslogsProviderImpl::RunCallbackIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled,
    base::TaskRunner* origin_runner,
    const ReadCompleteCallback& callback,
    LogDictionaryType* logs,
    std::string* zip_content) {
  DCHECK(!is_canceled.is_null() && !callback.is_null());

  if (is_canceled.Run()) {
    delete logs;
    delete zip_content;
    return;
  }

  // TODO(achuith@chromium.org): Maybe always run callback asynchronously?
  if (origin_runner->RunsTasksOnCurrentThread()) {
    callback.Run(logs, zip_content);
  } else {
    origin_runner->PostTask(FROM_HERE, base::Bind(callback, logs, zip_content));
  }
}

SyslogsProviderImpl* SyslogsProviderImpl::GetInstance() {
  return Singleton<SyslogsProviderImpl,
                   DefaultSingletonTraits<SyslogsProviderImpl> >::get();
}

SyslogsProvider* SyslogsProvider::GetInstance() {
  return SyslogsProviderImpl::GetInstance();
}

}  // namespace system
}  // namespace chromeos
