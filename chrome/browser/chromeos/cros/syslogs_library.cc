// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/syslogs_library.h"

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class SyslogsLibraryImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryImpl() {}
  virtual ~SyslogsLibraryImpl() {}

  virtual Handle RequestSyslogs(FilePath* tmpfilename,
                                CancelableRequestConsumerBase* consumer,
                                ReadCompleteCallback* callback) {
    // Register the callback request.
    scoped_refptr<CancelableRequest<ReadCompleteCallback> > request(
           new CancelableRequest<ReadCompleteCallback>(callback));
    AddRequest(request, consumer);

    // Schedule a task on the FILE thread which will then trigger a request
    // callback on the calling thread (e.g. UI) when complete.
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableMethod(
            this, &SyslogsLibraryImpl::ReadSyslogs, request, tmpfilename));

    return request->handle();
  }

 private:
  // Called from FILE thread.
  void ReadSyslogs(
      scoped_refptr<CancelableRequest<ReadCompleteCallback> > request,
      FilePath* tmpfilename) {
    if (request->canceled())
      return;

    LogDictionaryType* logs = NULL;
    if (CrosLibrary::Get()->EnsureLoaded()) {
      logs = chromeos::GetSystemLogs(tmpfilename);
    }

    // Will call the callback on the calling thread.
    request->ForwardResult(Tuple1<LogDictionaryType*>(logs));
  }

  DISALLOW_COPY_AND_ASSIGN(SyslogsLibraryImpl);
};

class SyslogsLibraryStubImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryStubImpl() {}
  virtual ~SyslogsLibraryStubImpl() {}

  virtual Handle RequestSyslogs(FilePath* tmpfilename,
                                CancelableRequestConsumerBase* consumer,
                                ReadCompleteCallback* callback) {
    if (callback)
      callback->Run(Tuple1<LogDictionaryType*>(NULL));

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

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. SyslogsLibraryImpl is a
// Singleton and won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::SyslogsLibraryImpl);
