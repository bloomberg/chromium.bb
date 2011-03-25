// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_browser_private_api.h"

#include "base/json/json_writer.h"
#include "base/task.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "content/browser/browser_thread.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_types.h"

class LocalFileSystemCallbackDispatcher
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit LocalFileSystemCallbackDispatcher(
      RequestLocalFileSystemFunction* function) : function_(function) {
    DCHECK(function_);
  }
  // fileapi::FileSystemCallbackDispatcher overrides.
  virtual void DidSucceed() OVERRIDE {
    NOTREACHED();
  }
  virtual void DidReadMetadata(const base::PlatformFileInfo& info) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool has_more) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidWrite(int64 bytes, bool complete) OVERRIDE {
    NOTREACHED();
  }
  virtual void DidOpenFileSystem(const std::string& name,
                                 const FilePath& path) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondSuccessOnUIThread,
            name,
            path));
  }
  virtual void DidFail(base::PlatformFileError error_code) OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(function_,
            &RequestLocalFileSystemFunction::RespondFailedOnUIThread,
            error_code));
  }
 private:
  RequestLocalFileSystemFunction* function_;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemCallbackDispatcher);
};

RequestLocalFileSystemFunction::RequestLocalFileSystemFunction() {
}

RequestLocalFileSystemFunction::~RequestLocalFileSystemFunction() {
}

bool RequestLocalFileSystemFunction::RunImpl() {
  fileapi::FileSystemOperation* operation =
      new fileapi::FileSystemOperation(
          new LocalFileSystemCallbackDispatcher(this),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          profile()->GetFileSystemContext(),
          NULL);
  GURL origin_url = source_url().GetOrigin();
  operation->OpenFileSystem(origin_url, fileapi::kFileSystemTypeLocal,
                            false);     // create
  // Will finish asynchronously.
  return true;
}

void RequestLocalFileSystemFunction::RespondSuccessOnUIThread(
    const std::string& name, const FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetString("name", name);
  dict->SetString("path", path.value());
  dict->SetInteger("error", base::PLATFORM_FILE_OK);
  SendResponse(true);
}

void RequestLocalFileSystemFunction::RespondFailedOnUIThread(
    base::PlatformFileError error_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  result_.reset(new DictionaryValue());
  DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(result_.get());
  dict->SetInteger("error", static_cast<int>(error_code));
  SendResponse(true);
}

