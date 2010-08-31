// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/webfilesystem_impl.h"

#include "chrome/common/file_system/file_system_dispatcher.h"
#include "chrome/common/child_thread.h"

using WebKit::WebString;
using WebKit::WebFileSystemCallbacks;

WebFileSystemImpl::WebFileSystemImpl() {
}

void WebFileSystemImpl::move(const WebString& src_path,
    const WebString& dest_path, WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Move(src_path, dest_path, callbacks);
}

void WebFileSystemImpl::copy(const WebKit::WebString& src_path,
    const WebKit::WebString& dest_path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Copy(src_path, dest_path, callbacks);
}

void WebFileSystemImpl::remove(const WebString& path,
    WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Remove(path, callbacks);
}

void WebFileSystemImpl::readMetadata(const WebString& path,
    WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadMetadata(path, callbacks);
}

void WebFileSystemImpl::createFile(const WebString& path,
    bool exclusive, WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(path, exclusive, false, callbacks);
}

void WebFileSystemImpl::createDirectory(const WebString& path,
    bool exclusive, WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(path, exclusive, true, callbacks);
}

void WebFileSystemImpl::fileExists(const WebString& path,
    WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(path, false, callbacks);
}

void WebFileSystemImpl::directoryExists(const WebString& path,
    WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(path, true, callbacks);
}

void WebFileSystemImpl::readDirectory(const WebString& path,
    WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadDirectory(path, callbacks);
}
