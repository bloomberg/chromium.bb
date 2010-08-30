// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/file_system_dispatcher.h"

#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileError;
using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;

FileSystemDispatcher::FileSystemDispatcher() {
}

FileSystemDispatcher::~FileSystemDispatcher() {
  // Make sure we fire all the remaining callbacks.
  for (IDMap<WebFileSystemCallbacks>::iterator iter(&callbacks_);
       !iter.IsAtEnd();
       iter.Advance()) {
    int callbacks_id = iter.GetCurrentKey();
    WebFileSystemCallbacks* callbacks = iter.GetCurrentValue();
    DCHECK(callbacks);
    callbacks_.Remove(callbacks_id);
    callbacks->didFail(WebKit::WebFileErrorAbort);
  }
}

bool FileSystemDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FileSystemDispatcher, msg)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_Succeeded, DidSucceed)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_Failed, DidFail)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void FileSystemDispatcher::Move(
    const string16& src_path, const string16& dest_path,
    WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Move(request_id, src_path, dest_path));
}

void FileSystemDispatcher::DidSucceed(int request_id) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks_.Remove(request_id);
  callbacks->didSucceed();
}

void FileSystemDispatcher::DidFail(int request_id, int code) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks_.Remove(request_id);
  callbacks->didFail(static_cast<WebFileError>(code));
}
