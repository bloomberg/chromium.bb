// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_LOGGING_H_
#define IPC_IPC_LOGGING_H_

#include "ipc/ipc_message.h"  // For IPC_MESSAGE_LOG_ENABLED.

#ifdef IPC_MESSAGE_LOG_ENABLED

#include <vector>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/waitable_event_watcher.h"

namespace IPC {

class Message;

// One instance per process.  Needs to be created on the main thread (the UI
// thread in the browser) but OnPreDispatchMessage/OnPostDispatchMessage
// can be called on other threads.
class Logging {
 public:
  // Implemented by consumers of log messages.
  class Consumer {
   public:
    virtual void Log(const LogData& data) = 0;
  };

  void SetConsumer(Consumer* consumer);

  ~Logging();
  static Logging* current();

  // Enable and Disable are NOT cross-process; they only affect the
  // current thread/process.  If you want to modify the value for all
  // processes, perhaps your intent is to call
  // g_browser_process->SetIPCLoggingEnabled().
  void Enable();
  void Disable();
  bool Enabled() const { return enabled_; }

  // Called by child processes to give the logger object the channel to send
  // logging data to the browser process.
  void SetIPCSender(Message::Sender* sender);

  // Called in the browser process when logging data from a child process is
  // received.
  void OnReceivedLoggingMessage(const Message& message);

  void OnSendMessage(Message* message, const std::string& channel_id);
  void OnPreDispatchMessage(const Message& message);
  void OnPostDispatchMessage(const Message& message,
                             const std::string& channel_id);

  // Returns the name of the logging enabled/disabled events so that the
  // sandbox can add them to to the policy.  If true, gets the name of the
  // enabled event, if false, gets the name of the disabled event.
  static std::wstring GetEventName(bool enabled);

  // Like the *MsgLog functions declared for each message class, except this
  // calls the correct one based on the message type automatically.  Defined in
  // ipc_logging.cc.
  static void GetMessageText(uint32 type, std::wstring* name,
                             const Message* message, std::wstring* params);

  typedef void (*LogFunction)(uint32 type,
                             std::wstring* name,
                             const Message* msg,
                             std::wstring* params);

  static void SetLoggerFunctions(LogFunction *functions);

 private:
  friend struct DefaultSingletonTraits<Logging>;
  Logging();

  void OnSendLogs();
  void Log(const LogData& data);

  bool enabled_;
  bool enabled_on_stderr_;  // only used on POSIX for now

  std::vector<LogData> queued_logs_;
  bool queue_invoke_later_pending_;

  Message::Sender* sender_;
  MessageLoop* main_thread_;

  Consumer* consumer_;

  static LogFunction *log_function_mapping_;
};

}  // namespace IPC

#endif // IPC_MESSAGE_LOG_ENABLED

#endif  // IPC_IPC_LOGGING_H_
