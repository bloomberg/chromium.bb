// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/fileapi/webfilesystem_impl.h"

#include "base/bind.h"
#include "content/child/child_thread.h"
#include "content/child/fileapi/file_system_dispatcher.h"
#include "content/child/fileapi/webfilesystem_callback_adapters.h"
#include "content/child/fileapi/webfilewriter_impl.h"
#include "third_party/WebKit/public/web/WebFileSystemCallbacks.h"
#include "third_party/WebKit/public/platform/WebFileInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

WebFileSystemImpl::WebFileSystemImpl() {
}

void WebFileSystemImpl::move(const WebURL& src_path,
                             const WebURL& dest_path,
                             WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Move(GURL(src_path),
                   GURL(dest_path),
                   base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::copy(const WebURL& src_path,
                             const WebURL& dest_path,
                             WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Copy(GURL(src_path),
                   GURL(dest_path),
                   base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::remove(const WebURL& path,
                               WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Remove(
      GURL(path),
      false /* recursive */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::removeRecursively(const WebURL& path,
                                          WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Remove(
      GURL(path),
      true /* recursive */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::readMetadata(const WebURL& path,
                                     WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadMetadata(
      GURL(path),
      base::Bind(&ReadMetadataCallbackAdapter, callbacks),
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::createFile(const WebURL& path,
                                   bool exclusive,
                                   WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(
      GURL(path), exclusive, false /* directory */, false /* recursive */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::createDirectory(const WebURL& path,
                                        bool exclusive,
                                        WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Create(
      GURL(path), exclusive, true /* directory */, false /* recursive */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::fileExists(const WebURL& path,
                                   WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(
      GURL(path), false /* directory */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::directoryExists(const WebURL& path,
                                        WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->Exists(
      GURL(path), true /* directory */,
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

void WebFileSystemImpl::readDirectory(const WebURL& path,
                                      WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->ReadDirectory(
      GURL(path),
      base::Bind(&ReadDirectoryCallbackAdapater, callbacks),
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

WebKit::WebFileWriter* WebFileSystemImpl::createFileWriter(
    const WebURL& path, WebKit::WebFileWriterClient* client) {
  return new WebFileWriterImpl(GURL(path), client);
}

void WebFileSystemImpl::createSnapshotFileAndReadMetadata(
    const WebKit::WebURL& path,
    WebKit::WebFileSystemCallbacks* callbacks) {
  FileSystemDispatcher* dispatcher =
      ChildThread::current()->file_system_dispatcher();
  dispatcher->CreateSnapshotFile(
      GURL(path),
      base::Bind(&CreateSnapshotFileCallbackAdapter, callbacks),
      base::Bind(&FileStatusCallbackAdapter, callbacks));
}

}  // namespace content
