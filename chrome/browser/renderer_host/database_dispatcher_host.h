// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_

#include "base/file_path.h"

class MessageLoop;
class ResourceMessageFilter;

namespace IPC {
class Message;
}

class DatabaseDispatcherHost {
 public:
  DatabaseDispatcherHost(const FilePath& profile_path,
                         ResourceMessageFilter* resource_message_filter);
  ~DatabaseDispatcherHost() {}

  // Returns true iff the message is HTML5 DB related and was processed.
  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

  // Processes the request to return a handle to the given DB file.
  void OnDatabaseOpenFile(const FilePath& file_name,
                          int desired_flags,
                          int32 message_id);

  // Processes the request to delete the given DB file.
  void OnDatabaseDeleteFile(const FilePath& file_name,
                            const bool& sync_dir,
                            int32 message_id);

  // Processes the request to return the attributes of the given DB file.
  void OnDatabaseGetFileAttributes(const FilePath& file_name,
                                   int32 message_id);

  // Processes the request to return the size of the given file.
  void OnDatabaseGetFileSize(const FilePath& file_name,
                             int32 message_id);

 private:
  // Determines if the message is HTML5 DB related.
  bool IsDBMessage(const IPC::Message& message);

  // Returns the directory where all DB files are stored.
  FilePath GetDBDir();

  // Returns the absolute name of the given DB file.
  FilePath GetDBFileFullPath(const FilePath& file_name);

  // The user data directory.
  FilePath profile_path_;

  // The ResourceMessageFilter instance of this renderer process.
  ResourceMessageFilter* resource_message_filter_;

  // The message loop of the file thread.
  MessageLoop* file_thread_message_loop_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DATABASE_DISPATCHER_HOST_H_
