// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/forwarding_net_log.h"

#include "base/lock.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace net {

// Reference-counted wrapper, so we can use PostThread and it can safely
// outlive the parent ForwardingNetLog.
class ForwardingNetLog::Core
    : public base::RefCountedThreadSafe<ForwardingNetLog::Core> {
 public:
  Core(NetLog* impl, MessageLoop* loop) : impl_(impl), loop_(loop) {
    DCHECK(impl);
    DCHECK(loop);
  }

  // Called once the parent ForwardingNetLog is being destroyed. It
  // is invalid to access |loop_| and |impl_| afterwards.
  void Orphan() {
    AutoLock l(lock_);
    loop_ = NULL;
    impl_ = NULL;
  }

  void AddEntry(EventType type,
                const base::TimeTicks& time,
                const Source& source,
                EventPhase phase,
                EventParameters* params) {
    AutoLock l(lock_);
    if (!loop_)
      return;  // Was orphaned.

    loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(
            this, &Core::AddEntryOnLoop, type, time, source, phase,
            scoped_refptr<EventParameters>(params)));
  }

 private:
  void AddEntryOnLoop(EventType type,
                      const base::TimeTicks& time,
                      const Source& source,
                      EventPhase phase,
                      scoped_refptr<EventParameters> params) {
    AutoLock l(lock_);
    if (!loop_)
      return;  // Was orphaned.

    DCHECK_EQ(MessageLoop::current(), loop_);

    // TODO(eroman): This shouldn't be necessary. See crbug.com/48806.
    NetLog::Source effective_source = source;
    if (effective_source.id == NetLog::Source::kInvalidId)
      effective_source.id = impl_->NextID();

    impl_->AddEntry(type, time, effective_source, phase, params);
  }

  Lock lock_;
  NetLog* impl_;
  MessageLoop* loop_;
};

ForwardingNetLog::ForwardingNetLog(NetLog* impl, MessageLoop* loop)
    : core_(new Core(impl, loop)) {
}

ForwardingNetLog::~ForwardingNetLog() {
  core_->Orphan();
}

void ForwardingNetLog::AddEntry(EventType type,
                                const base::TimeTicks& time,
                                const Source& source,
                                EventPhase phase,
                                EventParameters* params) {
  core_->AddEntry(type, time, source, phase, params);
}

uint32 ForwardingNetLog::NextID() {
  // Can't forward a synchronous API.
  CHECK(false) << "Not supported";
  return 0;
}

bool ForwardingNetLog::HasListener() const {
  // Can't forward a synchronous API.
  CHECK(false) << "Not supported";
  return false;
}

}  // namespace net

