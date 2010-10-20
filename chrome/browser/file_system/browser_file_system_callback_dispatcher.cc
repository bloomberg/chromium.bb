// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system/browser_file_system_callback_dispatcher.h"

#include "chrome/browser/file_system/file_system_dispatcher_host.h"
#include "chrome/common/render_messages.h"

BrowserFileSystemCallbackDispatcher::BrowserFileSystemCallbackDispatcher(
    FileSystemDispatcherHost* dispatcher_host, int request_id)
    : dispatcher_host_(dispatcher_host),
      request_id_(request_id) {
  DCHECK(dispatcher_host_);
}

BrowserFileSystemCallbackDispatcher::~BrowserFileSystemCallbackDispatcher() {}

void BrowserFileSystemCallbackDispatcher::DidSucceed() {
  dispatcher_host_->Send(new ViewMsg_FileSystem_DidSucceed(request_id_));
  dispatcher_host_->RemoveCompletedOperation(request_id_);
}

void BrowserFileSystemCallbackDispatcher::DidReadMetadata(
    const base::PlatformFileInfo& info) {
  dispatcher_host_->Send(new ViewMsg_FileSystem_DidReadMetadata(
      request_id_, info));
  dispatcher_host_->RemoveCompletedOperation(request_id_);
}

void BrowserFileSystemCallbackDispatcher::DidReadDirectory(
    const std::vector<base::file_util_proxy::Entry>& entries, bool has_more) {
  dispatcher_host_->Send(new ViewMsg_FileSystem_DidReadDirectory(
      request_id_, entries, has_more));
  dispatcher_host_->RemoveCompletedOperation(request_id_);
}

void BrowserFileSystemCallbackDispatcher::DidOpenFileSystem(
    const std::string&, const FilePath&) {
  NOTREACHED();
}

void BrowserFileSystemCallbackDispatcher::DidFail(
    base::PlatformFileError error_code) {
  dispatcher_host_->Send(new ViewMsg_FileSystem_DidFail(
      request_id_, error_code));
  dispatcher_host_->RemoveCompletedOperation(request_id_);
}

void BrowserFileSystemCallbackDispatcher::DidWrite(
    int64 bytes,
    bool complete) {
  dispatcher_host_->Send(new ViewMsg_FileSystem_DidWrite(
      request_id_, bytes, complete));
  if (complete)
    dispatcher_host_->RemoveCompletedOperation(request_id_);
}
