// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_BINDER_IPC_THREAD_H_
#define CHROMEOS_BINDER_IPC_THREAD_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "chromeos/chromeos_export.h"

namespace binder {

class CommandBroker;
class Driver;

// IpcThread manages binder-related resources (e.g. binder driver FD) and
// handles incoming binder commands.
class CHROMEOS_EXPORT IpcThread : public base::Thread,
                                  public base::MessageLoopForIO::Watcher {
 public:
  IpcThread();
  ~IpcThread() override;

  Driver* driver() { return driver_.get(); }
  CommandBroker* command_broker() { return command_broker_.get(); }
  bool initialized() const { return initialized_; }

  // Starts this thread.
  // Returns true on success.
  bool Start();

  // base::MessageLoopIO::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

 protected:
  // base::Thread overrides:
  void Init() override;
  void CleanUp() override;

 private:
  scoped_ptr<Driver> driver_;
  scoped_ptr<CommandBroker> command_broker_;
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> watcher_;
  bool initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(IpcThread);
};

}  // namespace binder

#endif  // CHROMEOS_BINDER_IPC_THREAD_H_
