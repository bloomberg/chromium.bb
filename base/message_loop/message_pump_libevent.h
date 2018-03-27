// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

// Declare structs we need from libevent.h rather than including it
struct event_base;
struct event;

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
class BASE_EXPORT MessagePumpLibevent : public MessagePump {
 public:
  // Used with WatchFileDescriptor to asynchronously monitor the I/O readiness
  // of a file descriptor.
  class Watcher {
   public:
    // Called from MessageLoop::Run when an FD can be read from/written to
    // without blocking
    virtual void OnFileCanReadWithoutBlocking(int fd) = 0;
    virtual void OnFileCanWriteWithoutBlocking(int fd) = 0;

   protected:
    virtual ~Watcher() = default;
  };

  // Object returned by WatchFileDescriptor to manage further watching.
  class FileDescriptorWatcher {
   public:
    explicit FileDescriptorWatcher(const Location& from_here);
    ~FileDescriptorWatcher();  // Implicitly calls StopWatchingFileDescriptor.

    // NOTE: These methods aren't called StartWatching()/StopWatching() to
    // avoid confusion with the win32 ObjectWatcher class.

    // Stop watching the FD, always safe to call.  No-op if there's nothing
    // to do.
    bool StopWatchingFileDescriptor();

    const Location& created_from_location() { return created_from_location_; }

   private:
    friend class MessagePumpLibevent;
    friend class MessagePumpLibeventTest;

    // Called by MessagePumpLibevent.
    void Init(std::unique_ptr<event> e);

    // Used by MessagePumpLibevent to take ownership of |event_|.
    std::unique_ptr<event> ReleaseEvent();

    void set_pump(MessagePumpLibevent* pump) { pump_ = pump; }
    MessagePumpLibevent* pump() const { return pump_; }

    void set_watcher(Watcher* watcher) { watcher_ = watcher; }

    void OnFileCanReadWithoutBlocking(int fd, MessagePumpLibevent* pump);
    void OnFileCanWriteWithoutBlocking(int fd, MessagePumpLibevent* pump);

    std::unique_ptr<event> event_;
    MessagePumpLibevent* pump_ = nullptr;
    Watcher* watcher_ = nullptr;
    // If this pointer is non-NULL, the pointee is set to true in the
    // destructor.
    bool* was_destroyed_ = nullptr;

    const Location created_from_location_;

    DISALLOW_COPY_AND_ASSIGN(FileDescriptorWatcher);
  };

  enum Mode {
    WATCH_READ = 1 << 0,
    WATCH_WRITE = 1 << 1,
    WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
  };

  MessagePumpLibevent();
  ~MessagePumpLibevent() override;

  // Registers |delegate| with the current thread's message loop so that its
  // methods are invoked when file descriptor |fd| becomes ready for reading or
  // writing (or both) without blocking.  |mode| selects ready for reading, for
  // writing, or both.  (See "enum Mode" above. TODO(unknown): nuke the
  // plethora of equivalent "enum Mode" declarations.)  |controller| manages
  // the lifetime of registrations. ("Registrations" are also ambiguously
  // called "events" in many places, for instance in libevent.)  It is an error
  // to use the same |controller| for different file descriptors; however, the
  // same controller can be reused to add registrations with a different
  // |mode|.  If |controller| is already attached to one or more registrations,
  // the new registration is added on to those.  If an error occurs while
  // calling this method, any registration previously attached to |controller|
  // is removed.  Returns true on success.  Must be called on the same thread
  // the message_pump is running on.
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FileDescriptorWatcher* controller,
                           Watcher* delegate);

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

 private:
  friend class MessagePumpLibeventTest;

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // Called by libevent to tell us a registered FD can be read/written to.
  static void OnLibeventNotification(int fd, short flags, void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);

  // This flag is set to false when Run should return.
  bool keep_running_;

  // This flag is set when inside Run.
  bool in_run_;

  // This flag is set if libevent has processed I/O events.
  bool processed_io_events_;

  // The time at which we should call DoDelayedWork.
  TimeTicks delayed_work_time_;

  // Libevent dispatcher.  Watches all sockets registered with it, and sends
  // readiness callbacks when a socket is ready for I/O.
  event_base* event_base_;

  // ... write end; ScheduleWork() writes a single byte to it
  int wakeup_pipe_in_;
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  int wakeup_pipe_out_;
  // ... libevent wrapper for read end
  event* wakeup_event_;

  ThreadChecker watch_file_descriptor_caller_checker_;
  DISALLOW_COPY_AND_ASSIGN(MessagePumpLibevent);
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
