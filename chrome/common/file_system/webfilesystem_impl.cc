// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/webfilesystem_impl.h"

#include "chrome/common/file_system/file_system_dispatcher.h"
#include "chrome/common/child_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebVector;

namespace {

WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError error_code) {
  switch (error_code) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return WebKit::WebFileErrorNoModificationAllowed;
    case base::PLATFORM_FILE_ERROR_FAILED:
      return WebKit::WebFileErrorInvalidState;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return WebKit::WebFileErrorAbort;
    default:
      return WebKit::WebFileErrorInvalidModification;
  }
}

class WebFileSystemCallbackDispatcherImpl
    : public fileapi::FileSystemCallbackDispatcher {
 public:
  explicit WebFileSystemCallbackDispatcherImpl(
      WebFileSystemCallbacks* callbacks)
      : callbacks_(callbacks) {
    DCHECK(callbacks_);
  }

  virtual ~WebFileSystemCallbackDispatcherImpl() {
  }

  // FileSystemCallbackDispatcher implementation
  virtual void DidSucceed() {
    callbacks_->didSucceed();
  }

  virtual void DidReadMetadata(const base::PlatformFileInfo& file_info) {
    WebFileInfo web_file_info;
    web_file_info.modificationTime = file_info.last_modified.ToDoubleT();
    callbacks_->didReadMetadata(web_file_info);
  }

  virtual void DidReadDirectory(
      const std::vector<base::file_util_proxy::Entry>& entries, bool has_more) {
    WebVector<WebFileSystemEntry> file_system_entries(entries.size());
    for (size_t i = 0; i < entries.size(); i++) {
      file_system_entries[i].name =
          webkit_glue::FilePathStringToWebString(entries[i].name);
      file_system_entries[i].isDirectory = entries[i].is_directory;
    }
    callbacks_->didReadDirectory(file_system_entries, has_more);
  }

  virtual void DidOpenFileSystem(const string16&, const FilePath&) {
    NOTREACHED();
  }

  virtual void DidFail(base::PlatformFileError error_code) {
    callbacks_->didFail(PlatformFileErrorToWebFileError(error_code));
  }

 private:
  WebFileSystemCallbacks* callbacks_;
};

} // namespace

WebFileSystemImpl::WebFileSystemImpl() {
}

void WebFileSystemImpl::move(const WebString& src_path,
                             const WebString& dest_path,
                             WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Move(webkit_glue::WebStringToFilePath(src_path),
                   webkit_glue::WebStringToFilePath(dest_path),
                   new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::copy(const WebString& src_path,
                             const WebString& dest_path,
                             WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Copy(webkit_glue::WebStringToFilePath(src_path),
                   webkit_glue::WebStringToFilePath(dest_path),
                   new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::remove(const WebString& path,
                               WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Remove(webkit_glue::WebStringToFilePath(path),
                     new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::readMetadata(const WebString& path,
                                     WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadMetadata(webkit_glue::WebStringToFilePath(path),
                           new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::createFile(const WebString& path,
                                   bool exclusive,
                                   WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(webkit_glue::WebStringToFilePath(path), exclusive, false,
                     false, new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::createDirectory(const WebString& path,
                                        bool exclusive,
                                        WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(webkit_glue::WebStringToFilePath(path), exclusive, true,
                     false, new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::fileExists(const WebString& path,
                                   WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(webkit_glue::WebStringToFilePath(path), false,
                     new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::directoryExists(const WebString& path,
                                        WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(webkit_glue::WebStringToFilePath(path), true,
                     new WebFileSystemCallbackDispatcherImpl(callbacks));
}

void WebFileSystemImpl::readDirectory(const WebString& path,
                                      WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadDirectory(webkit_glue::WebStringToFilePath(path),
                            new WebFileSystemCallbackDispatcherImpl(callbacks));
}
