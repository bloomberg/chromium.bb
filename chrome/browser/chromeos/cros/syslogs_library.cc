// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/syslogs_library.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

const char kContextFeedback[] = "feedback";
const char kContextSysInfo[] = "sysinfo";


class SyslogsLibraryImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryImpl() {}
  virtual ~SyslogsLibraryImpl() {}

  virtual Handle RequestSyslogs(
      bool compress_logs, bool add_feedback_context,
      CancelableRequestConsumerBase* consumer,
      ReadCompleteCallback* callback);

  // Reads system logs, compresses content if requested.
  // Called from FILE thread.
  void ReadSyslogs(
      scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
      bool compress_logs, bool add_feedback_context);

  void LoadCompressedLogs(const FilePath& zip_file,
                          std::string* zip_content);

  DISALLOW_COPY_AND_ASSIGN(SyslogsLibraryImpl);
};

class SyslogsLibraryStubImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryStubImpl() {}
  virtual ~SyslogsLibraryStubImpl() {}

  virtual Handle RequestSyslogs(bool compress_logs, bool add_feedback_context,
                                CancelableRequestConsumerBase* consumer,
                                ReadCompleteCallback* callback) {
    if (callback)
      callback->Run(Tuple2<LogDictionaryType*, std::string*>(NULL , NULL));

    return 0;
  }
};

// static
SyslogsLibrary* SyslogsLibrary::GetImpl(bool stub) {
  if (stub)
    return new SyslogsLibraryStubImpl();
  else
    return new SyslogsLibraryImpl();
}


CancelableRequestProvider::Handle SyslogsLibraryImpl::RequestSyslogs(
    bool compress_logs, bool add_feedback_context,
    CancelableRequestConsumerBase* consumer,
    ReadCompleteCallback* callback) {
  // Register the callback request.
  scoped_refptr<CancelableRequest<ReadCompleteCallback> > request(
         new CancelableRequest<ReadCompleteCallback>(callback));
  AddRequest(request, consumer);

  // Schedule a task on the FILE thread which will then trigger a request
  // callback on the calling thread (e.g. UI) when complete.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(
          this, &SyslogsLibraryImpl::ReadSyslogs, request,
          compress_logs, add_feedback_context));

  return request->handle();
}

// Called from FILE thread.
void SyslogsLibraryImpl::ReadSyslogs(
    scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
    bool compress_logs, bool add_feedback_context) {
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
  if (CrosLibrary::Get()->EnsureLoaded())
    logs = chromeos::GetSystemLogs(
        compress_logs ? &zip_file : NULL,
        add_feedback_context ? kContextFeedback : kContextSysInfo);

  std::string* zip_content = NULL;
  if (compress_logs) {
    // Load compressed logs.
    zip_content = new std::string();
    LoadCompressedLogs(zip_file, zip_content);
    file_util::Delete(zip_file, false);
  }

  // Will call the callback on the calling thread.
  request->ForwardResult(Tuple2<LogDictionaryType*,
                                std::string*>(logs, zip_content));
}


void SyslogsLibraryImpl::LoadCompressedLogs(const FilePath& zip_file,
                                            std::string* zip_content) {
  DCHECK(zip_content);
  if (!file_util::ReadFileToString(zip_file, zip_content)) {
    LOG(ERROR) << "Cannot read compressed logs file from " <<
        zip_file.value().c_str();
  }
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. SyslogsLibraryImpl is a
// Singleton and won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::SyslogsLibraryImpl);
