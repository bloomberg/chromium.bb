// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/file_system/file_system_dispatcher.h"

#include "base/file_util.h"
#include "chrome/common/child_thread.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemEntry.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/WebKit/chromium/public/WebVector.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFileError;
using WebKit::WebFileInfo;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFileSystemEntry;
using WebKit::WebVector;

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
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidSucceed, DidSucceed)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidReadDirectory, DidReadDirectory)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidReadMetadata, DidReadMetadata)
    IPC_MESSAGE_HANDLER(ViewMsg_FileSystem_DidFail, DidFail)
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

void FileSystemDispatcher::Copy(
    const string16& src_path, const string16& dest_path,
    WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Copy(request_id, src_path, dest_path));
}

void FileSystemDispatcher::Remove(
    const string16& path, WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Remove(request_id, path));
}

void FileSystemDispatcher::ReadMetadata(
    const string16& path, WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_ReadMetadata(request_id, path));
}

void FileSystemDispatcher::Create(
    const string16& path, bool exclusive, bool is_directory,
    WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Create(request_id, path, exclusive,
                                        is_directory));
}

void FileSystemDispatcher::Exists(
    const string16& path, bool is_directory,
    WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_Exists(request_id, path, is_directory));
}

void FileSystemDispatcher::ReadDirectory(
    const string16& path, WebFileSystemCallbacks* callbacks) {
  int request_id = callbacks_.Add(callbacks);
  ChildThread::current()->Send(
      new ViewHostMsg_FileSystem_ReadDirectory(request_id, path));
}

void FileSystemDispatcher::DidSucceed(int request_id) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks_.Remove(request_id);
  callbacks->didSucceed();
}

void FileSystemDispatcher::DidReadMetadata(int request_id,
    const base::PlatformFileInfo& file_info) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks_.Remove(request_id);
  WebFileInfo web_file_info;
  web_file_info.modificationTime = file_info.last_modified.ToDoubleT();
  callbacks->didReadMetadata(web_file_info);
}

void FileSystemDispatcher::DidReadDirectory(
    const ViewMsg_FileSystem_DidReadDirectory_Params& params) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(params.request_id);
  DCHECK(callbacks);
  if (!params.has_more)
    callbacks_.Remove(params.request_id);
  WebVector<WebFileSystemEntry> entries(params.entries.size());
  for (size_t i = 0; i < params.entries.size(); ++i) {
    entries[i].name = webkit_glue::FilePathToWebString(params.entries[i].name);
    entries[i].isDirectory = params.entries[i].is_directory;
  }
  callbacks->didReadDirectory(entries, params.has_more);
}

void FileSystemDispatcher::DidFail(int request_id, WebFileError code) {
  WebFileSystemCallbacks* callbacks = callbacks_.Lookup(request_id);
  DCHECK(callbacks);
  callbacks_.Remove(request_id);
  callbacks->didFail(code);
}
