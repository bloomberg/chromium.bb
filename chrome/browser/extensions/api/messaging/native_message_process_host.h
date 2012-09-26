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
  class ScopedNativeProcessClose;

#if defined(OS_WIN)
  typedef HANDLE FileHandle;
  typedef base::win::ScopedHandle ScopedFileHandle;
#else
  typedef int FileHandle;
  typedef file_util::ScopedFD ScopedFileHandle;
#endif  // defined(OS_WIN)

  typedef scoped_ptr_malloc<NativeMessageProcessHost, ScopedNativeProcessClose>
      ScopedHost;

  typedef base::Callback<void(ScopedHost host)> CreateCallback;

  // Append any new types to the end. Changing the ordering will break native
  // apps.
  enum MessageType {
    TYPE_SEND_MESSAGE_REQUEST,  // Used when an extension is sending a one-off
                                // message to a native app.
    TYPE_SEND_MESSAGE_RESPONSE,  // Used by a native app to respond to a one-off
                                 // message.
    TYPE_CONNECT,  // Used when an extension wants to establish a persistent
                   // connection with a native app.
    TYPE_CONNECT_MESSAGE,  // Used for messages after a connection has already
                           // been established.
    NUM_MESSAGE_TYPES  // The number of types of messages.
  };

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

  // Desctruction functor that ensures a NativeMessageProcessHost is destroyed
  // on the FILE thread.
  class ScopedNativeProcessClose {
   public:
    inline void operator()(extensions::NativeMessageProcessHost* x) const {
      content::BrowserThread::DeleteSoon(content::BrowserThread::FILE,
                                        FROM_HERE, x);
    }
  };


  virtual ~NativeMessageProcessHost();

  // |type| must be TYPE_CONNECT or TYPE_SEND_MESSAGE_REQUEST. |callback| will
  // be called with an empty ScopedHost on error.
  static void Create(base::WeakPtr<Client> weak_client_ui,
                     const std::string& native_app_name,
                     const std::string& connection_message,
                     int destination_port,
                     MessageType type,
                     CreateCallback callback);

  // Create a NativeMessageProcessHost using the specified launcher. This allows
  // for easy testing.
  static void CreateWithLauncher(base::WeakPtr<Client> weak_client_ui,
                                 const std::string& native_app_name,
                                 const std::string& connection_message,
                                 int destination_port,
                                 MessageType type,
                                 CreateCallback callback,
                                 const NativeProcessLauncher& launcher);

  // TYPE_SEND_MESSAGE_REQUEST will be sent via the connection message in
  // NativeMessageProcessHost::Create, so only TYPE_CONNECT_MESSAGE is expected.
  void Send(const std::string& json) {
    SendImpl(TYPE_CONNECT_MESSAGE, json);
  }

  // Try and read a single message from |read_file_|. This should only be called
  // in unittests when you know there is data in the file.
  void ReadNowForTesting();

 private:
  NativeMessageProcessHost(base::WeakPtr<Client> weak_client_ui,
                           int destination_port,
                           base::ProcessHandle native_process_handle,
                           FileHandle read_fd,
                           FileHandle write_fd,
                           bool is_send_message);

  // Initialize any IO watching that needs to occur between the native process.
  void InitIO();

  // Send a message to the native process with the specified type and payload.
  void SendImpl(MessageType type, const std::string& json);

  // Write a message/data to the native process.
  bool WriteMessage(MessageType type, const std::string& message);
  bool WriteData(FileHandle file, const char* data, size_t bytes_to_write);

  // Read a message/data from the native process.
  bool ReadMessage(MessageType* type, std::string* messgae);
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

  // The id of the port on the other side of this connection. This is passed to
  // |weak_client_ui_| when posting messages.
  int destination_port_;

  base::ProcessHandle native_process_handle_;

  FileHandle read_file_;
  FileHandle write_file_;
  ScopedFileHandle scoped_read_file_;
  ScopedFileHandle scoped_write_file_;

  // Only looking for one response.
  bool is_send_message_;

  DISALLOW_COPY_AND_ASSIGN(NativeMessageProcessHost);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGE_PROCESS_HOST_H__
