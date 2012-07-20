// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/callback_util.h"

#include "base/bind.h"
#include "base/synchronization/lock.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"

namespace media {

// Executes the given closure if and only if the closure returned by
// GetClosure() has been executed exactly |count| times.
//
// |done_cb| will be executed on the same thread that created the CountingCB.
class CountingCB : public base::RefCountedThreadSafe<CountingCB> {
 public:
  CountingCB(int count, const base::Closure& done_cb)
      : message_loop_(base::MessageLoopProxy::current()),
        count_(count),
        done_cb_(done_cb) {
  }

  // Returns a closure bound to this object.
  base::Closure GetClosure() {
    return base::Bind(&CountingCB::OnCallback, this);
  }

 protected:
  friend class base::RefCountedThreadSafe<CountingCB>;
  virtual ~CountingCB() {}

 private:
  void OnCallback() {
    {
      base::AutoLock l(lock_);
      count_--;
      DCHECK_GE(count_, 0) << "CountingCB executed too many times";
      if (count_ != 0)
        return;
    }

    if (!message_loop_->BelongsToCurrentThread()) {
      message_loop_->PostTask(FROM_HERE, done_cb_);
      return;
    }

    done_cb_.Run();
  }

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::Lock lock_;
  int count_;
  base::Closure done_cb_;

  DISALLOW_COPY_AND_ASSIGN(CountingCB);
};

static void OnSeriesCallback(
    scoped_refptr<base::MessageLoopProxy> message_loop,
    scoped_ptr<std::queue<ClosureFunc> > closures,
    const base::Closure& done_cb) {
  if (!message_loop->BelongsToCurrentThread()) {
    message_loop->PostTask(FROM_HERE, base::Bind(
        &OnSeriesCallback, message_loop, base::Passed(&closures), done_cb));
    return;
  }

  if (closures->empty()) {
    done_cb.Run();
    return;
  }

  ClosureFunc cb = closures->front();
  closures->pop();
  cb.Run(base::Bind(
      &OnSeriesCallback, message_loop, base::Passed(&closures), done_cb));
}

void RunInSeries(scoped_ptr<std::queue<ClosureFunc> > closures,
                 const base::Closure& done_cb) {
  OnSeriesCallback(base::MessageLoopProxy::current(),
                   closures.Pass(), done_cb);
}

static void OnStatusCallback(
    scoped_refptr<base::MessageLoopProxy> message_loop,
    scoped_ptr<std::queue<PipelineStatusCBFunc> > status_cbs,
    const PipelineStatusCB& done_cb,
    PipelineStatus last_status) {
  if (!message_loop->BelongsToCurrentThread()) {
    message_loop->PostTask(FROM_HERE, base::Bind(
        &OnStatusCallback, message_loop, base::Passed(&status_cbs), done_cb,
        last_status));
    return;
  }

  if (status_cbs->empty() || last_status != PIPELINE_OK) {
    done_cb.Run(last_status);
    return;
  }

  PipelineStatusCBFunc status_cb = status_cbs->front();
  status_cbs->pop();
  status_cb.Run(base::Bind(
      &OnStatusCallback, message_loop, base::Passed(&status_cbs), done_cb));
}

void RunInSeriesWithStatus(
    scoped_ptr<std::queue<PipelineStatusCBFunc> > status_cbs,
    const PipelineStatusCB& done_cb) {
  OnStatusCallback(base::MessageLoopProxy::current(),
                   status_cbs.Pass(), done_cb, PIPELINE_OK);
}

void RunInParallel(scoped_ptr<std::queue<ClosureFunc> > closures,
                   const base::Closure& done_cb) {
  if (closures->empty()) {
    done_cb.Run();
    return;
  }

  scoped_refptr<CountingCB> counting_cb =
      new CountingCB(closures->size(), done_cb);
  while (!closures->empty()) {
    closures->front().Run(counting_cb->GetClosure());
    closures->pop();
  }
}

}  // namespace media
