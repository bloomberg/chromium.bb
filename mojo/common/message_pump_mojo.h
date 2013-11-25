// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_MESSAGE_PUMP_MOJO_H_
#define MOJO_COMMON_MESSAGE_PUMP_MOJO_H_

#include <map>

#include "base/message_loop/message_pump.h"
#include "base/time/time.h"
#include "mojo/common/mojo_common_export.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace common {

class MessagePumpMojoHandler;

// Mojo implementation of MessagePump.
class MOJO_COMMON_EXPORT MessagePumpMojo : public base::MessagePump {
 public:
  MessagePumpMojo();
  virtual ~MessagePumpMojo();

  // Registers a MessagePumpMojoHandler for the specified handle. Only one
  // handler can be registered for a specified handle.
  void AddHandler(MessagePumpMojoHandler* handler,
                  MojoHandle handle,
                  MojoWaitFlags wait_flags,
                  base::TimeTicks deadline);

  void RemoveHandler(MojoHandle handle);

  // MessagePump:
  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(
      const base::TimeTicks& delayed_work_time) OVERRIDE;

 private:
  struct RunState;
  struct WaitState;

  // Creates a MessagePumpMojoHandler and the set of MojoWaitFlags it was
  // registered with.
  struct Handler {
    Handler() : handler(NULL), wait_flags(MOJO_WAIT_FLAG_NONE), id(0) {}

    MessagePumpMojoHandler* handler;
    MojoWaitFlags wait_flags;
    base::TimeTicks deadline;
    // See description of |MessagePumpMojo::next_handler_id_| for details.
    int id;
  };

  typedef std::map<MojoHandle, Handler> HandleToHandler;

  // Services the set of handles ready. If |block| is true this waits for a
  // handle to become ready, otherwise this does not block.
  void DoInternalWork(bool block);

  // Removes the first invalid handle. This is called if MojoWaitMany finds an
  // invalid handle.
  void RemoveFirstInvalidHandle(const WaitState& wait_state);

  void SignalControlPipe();

  WaitState GetWaitState() const;

  // Returns the deadline for the call to MojoWaitMany().
  MojoDeadline GetDeadlineForWait() const;

  // If non-NULL we're running (inside Run()). Member is reference to value on
  // stack.
  RunState* run_state_;

  HandleToHandler handlers_;

  // An ever increasing value assigned to each Handler::id. Used to detect
  // uniqueness while notifying. That is, while notifying expired timers we copy
  // |handlers_| and only notify handlers whose id match. If the id does not
  // match it means the handler was removed then added so that we shouldn't
  // notify it.
  int next_handler_id_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpMojo);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_MESSAGE_PUMP_MOJO_H_
