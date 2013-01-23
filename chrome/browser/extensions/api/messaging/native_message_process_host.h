// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H_

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
class NativeProcessLauncher;

// Manages the native side of a connection between an extension and a native
// process.
//
// This class must only be created, called, and deleted on the FILE thread.
// Public methods typically accept callbacks which will be invoked on the UI
// thread.
class NativeMessageProcessHost
#if defined(OS_WIN)
    : public MessageLoopForIO::IOHandler {
#else
    : public MessageLoopForIO::Watcher {
#endif  // defined(OS_WIN)
 public:
#if defined(OS_WIN)
  typedef HANDLE FileHandle;
  typedef base::win::ScopedHandle ScopedFileHandle;
#else
  typedef int FileHandle;
  typedef file_util::ScopedFD ScopedFileHandle;
#endif  // defined(OS_WIN)

  // Interface for classes that which to recieve messages from the native
  // process.
  class Client {
   public:
    virtual ~Client() {}
    // Called on the UI thread.
    virtual void PostMessageFromNativeProcess(int port_id,
                                              const std::string& message) = 0;
    virtual void CloseChannel(int port_id, bool error) = 0;
  };

  virtual ~NativeMessageProcessHost();

  static scoped_ptr<NativeMessageProcessHost> Create(
      base::WeakPtr<Client> weak_client_ui,
      const std::string& native_host_name,
      int destination_port);

  // Create using specified |launcher|. Used in tests.
  static scoped_ptr<NativeMessageProcessHost> CreateWithLauncher(
      base::WeakPtr<Client> weak_client_ui,
      const std::string& native_host_name,
      int destination_port,
      scoped_ptr<NativeProcessLauncher> launcher);

  // Send a message with the specified payload.
  void Send(const std::string& json);

  // Try and read a single message from |read_file_|. This should only be called
  // in unittests when you know there is data in the file.
  void ReadNowForTesting();

 private:
  NativeMessageProcessHost(base::WeakPtr<Client> weak_client_ui,
                           const std::string& native_host_name,
                           int destination_port,
                           scoped_ptr<NativeProcessLauncher> launcher);

  // Starts the host process.
  void LaunchHostProcess(scoped_ptr<NativeProcessLauncher> launcher);

  // Initialize any IO watching that needs to occur between the native process.
  void InitIO();

  // Write a message/data to the native process.
  bool WriteMessage(const std::string& message);
  bool WriteData(FileHandle file, const char* data, size_t bytes_to_write);

  // Read a message/data from the native process.
  bool ReadMessage(std::string* message);
  bool ReadData(FileHandle file, char* data, size_t bytes_to_write);

#if defined(OS_POSIX)
  // MessageLoopForIO::Watcher
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE;
  // We don't need to watch for writes.
  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {}

  MessageLoopForIO::FileDescriptorWatcher read_watcher_;
#endif  // defined(OS_POSIX)

#if defined(OS_WIN)
  // MessageLoopForIO::IOHandler
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered,
                             DWORD error) OVERRIDE;

  MessageLoopForIO::IOContext read_context_;
  MessageLoopForIO::IOContext write_context_;
#endif  // defined(OS_WIN)


  // The Client messages will be posted to. Should only be accessed from the
  // UI thread.
  base::WeakPtr<Client> weak_client_ui_;

  // Name of the native messaging host.
  std::string native_host_name_;

  // The id of the port on the other side of this connection. This is passed to
  // |weak_client_ui_| when posting messages.
  int destination_port_;

  base::ProcessHandle native_process_handle_;

  FileHandle read_file_;
  FileHandle write_file_;
  ScopedFileHandle scoped_read_file_;
  ScopedFileHandle scoped_write_file_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessageProcessHost);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H__
