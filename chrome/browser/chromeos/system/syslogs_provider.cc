// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/syslogs_provider.h"

#include <functional>
#include <set>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/memory_details.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {
namespace system {
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
  size_t equal_sign = data->find("=");
  if (equal_sign == std::string::npos)
    return std::string("");
  std::string key = data->substr(0, equal_sign);
  data->erase(0, equal_sign);
  if (data->size() > 0) {
    // erase the equal to sign also
    data->erase(0,1);
    return key;
  }
  return std::string();
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
  TrimString(*data, " \t", data);

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
  } else { // single line value
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

LogDictionaryType* GetSystemLogs(FilePath* zip_file_name,
                                 const std::string& context) {
  // Create the temp file, logs will go here
  FilePath temp_filename;

  if (!file_util::CreateTemporaryFile(&temp_filename))
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
  bool read_success = file_util::ReadFileToString(temp_filename,
                                                  &data);
  // if we were using an internal temp file, the user does not need the
  // logs to stay past the ReadFile call - delete the file
  file_util::Delete(temp_filename, false);

  if (!read_success)
    return NULL;

  // Parse the return data into a dictionary
  LogDictionaryType* logs = new LogDictionaryType();
  while (data.length() > 0) {
    std::string key = ReadKey(&data);
    TrimWhitespaceASCII(key, TRIM_ALL, &key);
    if (!key.empty()) {
      std::string value = ReadValue(&data);
      if (IsStringUTF8(value)) {
        TrimWhitespaceASCII(value, TRIM_ALL, &value);
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
  virtual Handle RequestSyslogs(
      bool compress_logs,
      SyslogsContext context,
      CancelableRequestConsumerBase* consumer,
      const ReadCompleteCallback& callback);

  // Reads system logs, compresses content if requested.
  // Called from FILE thread.
  void ReadSyslogs(
      scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
      bool compress_logs,
      SyslogsContext context);

  // Loads compressed logs and writes into |zip_content|.
  void LoadCompressedLogs(const FilePath& zip_file,
                          std::string* zip_content);

  static SyslogsProviderImpl* GetInstance();

 private:
  friend struct DefaultSingletonTraits<SyslogsProviderImpl>;

  SyslogsProviderImpl();

  // Gets syslogs context string from the enum value.
  const char* GetSyslogsContextString(SyslogsContext context);

  DISALLOW_COPY_AND_ASSIGN(SyslogsProviderImpl);
};

SyslogsProviderImpl::SyslogsProviderImpl() {
}

CancelableRequestProvider::Handle SyslogsProviderImpl::RequestSyslogs(
    bool compress_logs,
    SyslogsContext context,
    CancelableRequestConsumerBase* consumer,
    const ReadCompleteCallback& callback) {
  // Register the callback request.
  scoped_refptr<CancelableRequest<ReadCompleteCallback> > request(
         new CancelableRequest<ReadCompleteCallback>(callback));
  AddRequest(request, consumer);

  // Schedule a task on the FILE thread which will then trigger a request
  // callback on the calling thread (e.g. UI) when complete.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &SyslogsProviderImpl::ReadSyslogs, request,
          compress_logs, context));

  return request->handle();
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
  SyslogsMemoryHandler(
      scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
      LogDictionaryType* logs,
      std::string* zip_content)
      : request_(request),
        logs_(logs),
        zip_content_(zip_content) {
  }

  virtual void OnDetailsAvailable() OVERRIDE {
    const ProcessData& chrome = processes()[0];  // Chrome is the first entry.
    // Process info, sorted by memory used (highest to lowest).
    typedef std::pair<int, std::string> ProcInfo;
    typedef std::set<ProcInfo, std::greater<ProcInfo> > ProcInfoSet;
    ProcInfoSet process_info;
    for (ProcessMemoryInformationList::const_iterator iter1 =
             chrome.processes.begin();
         iter1 != chrome.processes.end(); ++iter1) {
      std::string process_string(
          ProcessMemoryInformation::GetFullTypeNameInEnglish(
              iter1->type, iter1->renderer_type));
      if (!iter1->titles.empty()) {
        std::string titles(" [");
        for (std::vector<string16>::const_iterator iter2 =
                 iter1->titles.begin();
             iter2 != iter1->titles.end(); ++iter2) {
          if (iter2 != iter1->titles.begin())
            titles += "|";
          titles += UTF16ToUTF8(*iter2);
        }
        titles += "]";
        process_string += titles;
      }
      // Use private working set for memory used calculation.
      int ws_mbytes = static_cast<int>(iter1->working_set.priv) / 1024;
      process_info.insert(std::make_pair(ws_mbytes, process_string));
    }
    // Add one line for each reverse-sorted entry.
    std::string mem_string;
    for (ProcInfoSet::iterator iter = process_info.begin();
         iter != process_info.end(); ++iter) {
      mem_string +=
          iter->second + base::StringPrintf(": %d MB", iter->first) + "\n";
    }
    (*logs_)["mem_usage"] = mem_string;
    // This will call the callback on the calling thread.
    request_->ForwardResult(logs_, zip_content_);
  }

 private:
  virtual ~SyslogsMemoryHandler() {}

  scoped_refptr<CancelableRequest<ReadCompleteCallback> > request_;
  LogDictionaryType* logs_;
  std::string* zip_content_;
  DISALLOW_COPY_AND_ASSIGN(SyslogsMemoryHandler);
};

// Called from FILE thread.
void SyslogsProviderImpl::ReadSyslogs(
    scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
    bool compress_logs,
    SyslogsContext context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (request->canceled())
    return;

  if (compress_logs && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCompressSystemFeedback))
    compress_logs = false;

  // Create temp file.
  FilePath zip_file;
  if (compress_logs && !file_util::CreateTemporaryFile(&zip_file)) {
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
    file_util::Delete(zip_file, false);
  }

  // SyslogsMemoryHandler will clean itself up.
  // SyslogsMemoryHandler::OnDetailsAvailable() will modify |logs| and call
  // request->ForwardResult(logs, zip_content).
  scoped_refptr<SyslogsMemoryHandler>
      handler(new SyslogsMemoryHandler(request, logs, zip_content));
  handler->StartFetch();
}

void SyslogsProviderImpl::LoadCompressedLogs(const FilePath& zip_file,
                                            std::string* zip_content) {
  DCHECK(zip_content);
  if (!file_util::ReadFileToString(zip_file, zip_content)) {
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

SyslogsProviderImpl* SyslogsProviderImpl::GetInstance() {
  return Singleton<SyslogsProviderImpl,
                   DefaultSingletonTraits<SyslogsProviderImpl> >::get();
}

SyslogsProvider* SyslogsProvider::GetInstance() {
  return SyslogsProviderImpl::GetInstance();
}

}  // namespace system
}  // namespace chromeos

// Allows InvokeLater without adding refcounting. SyslogsProviderImpl is a
// Singleton and won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::system::SyslogsProviderImpl);
