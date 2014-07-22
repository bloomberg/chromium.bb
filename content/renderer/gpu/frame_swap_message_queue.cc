// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/frame_swap_message_queue.h"

#include <limits>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "ipc/ipc_message.h"

using std::vector;

namespace content {

class FrameSwapMessageSubQueue {
 public:
  FrameSwapMessageSubQueue() {}
  virtual ~FrameSwapMessageSubQueue() {}
  virtual bool Empty() const = 0;
  virtual void QueueMessage(int source_frame_number,
                            scoped_ptr<IPC::Message> msg,
                            bool* is_first) = 0;
  virtual void DrainMessages(int source_frame_number,
                             ScopedVector<IPC::Message>* messages) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameSwapMessageSubQueue);
};

namespace {

// Queue specific to MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE.
class SendMessageScopeImpl : public FrameSwapMessageQueue::SendMessageScope {
 public:
  SendMessageScopeImpl(base::Lock* lock) : auto_lock_(*lock) {}
  virtual ~SendMessageScopeImpl() OVERRIDE {}

 private:
  base::AutoLock auto_lock_;
};

class VisualStateQueue : public FrameSwapMessageSubQueue {
 public:
  VisualStateQueue() {}

  virtual ~VisualStateQueue() {
    for (VisualStateQueueMap::iterator i = queue_.begin(); i != queue_.end();
         i++) {
      STLDeleteElements(&i->second);
    }
  }

  virtual bool Empty() const OVERRIDE { return queue_.empty(); }

  virtual void QueueMessage(int source_frame_number,
                            scoped_ptr<IPC::Message> msg,
                            bool* is_first) OVERRIDE {
    if (is_first)
      *is_first = (queue_.count(source_frame_number) == 0);

    queue_[source_frame_number].push_back(msg.release());
  }

  virtual void DrainMessages(int source_frame_number,
                             ScopedVector<IPC::Message>* messages) OVERRIDE {
    VisualStateQueueMap::iterator end = queue_.upper_bound(source_frame_number);
    for (VisualStateQueueMap::iterator i = queue_.begin(); i != end; i++) {
      DCHECK(i->first <= source_frame_number);
      messages->insert(messages->end(), i->second.begin(), i->second.end());
      i->second.clear();
    }
    queue_.erase(queue_.begin(), end);
  }

 private:
  typedef std::map<int, std::vector<IPC::Message*> > VisualStateQueueMap;
  VisualStateQueueMap queue_;

  DISALLOW_COPY_AND_ASSIGN(VisualStateQueue);
};

// Queue specific to MESSAGE_DELIVERY_POLICY_WITH_NEXT_SWAP.
class SwapQueue : public FrameSwapMessageSubQueue {
 public:
  SwapQueue() {}
  virtual bool Empty() const OVERRIDE { return queue_.empty(); }

  virtual void QueueMessage(int source_frame_number,
                            scoped_ptr<IPC::Message> msg,
                            bool* is_first) OVERRIDE {
    if (is_first)
      *is_first = Empty();
    queue_.push_back(msg.release());
  }

  virtual void DrainMessages(int source_frame_number,
                             ScopedVector<IPC::Message>* messages) OVERRIDE {
    messages->insert(messages->end(), queue_.begin(), queue_.end());
    queue_.weak_clear();
  }

 private:
  ScopedVector<IPC::Message> queue_;

  DISALLOW_COPY_AND_ASSIGN(SwapQueue);
};

}  // namespace

FrameSwapMessageQueue::FrameSwapMessageQueue()
    : visual_state_queue_(new VisualStateQueue()),
      swap_queue_(new SwapQueue()) {
}

FrameSwapMessageQueue::~FrameSwapMessageQueue() {
}

bool FrameSwapMessageQueue::Empty() const {
  base::AutoLock lock(lock_);
  return next_drain_messages_.empty() && visual_state_queue_->Empty() &&
         swap_queue_->Empty();
}

FrameSwapMessageSubQueue* FrameSwapMessageQueue::GetSubQueue(
    MessageDeliveryPolicy policy) {
  switch (policy) {
    case MESSAGE_DELIVERY_POLICY_WITH_NEXT_SWAP:
      return swap_queue_.get();
      break;
    case MESSAGE_DELIVERY_POLICY_WITH_VISUAL_STATE:
      return visual_state_queue_.get();
      break;
  }
  NOTREACHED();
  return NULL;
}

void FrameSwapMessageQueue::QueueMessageForFrame(MessageDeliveryPolicy policy,
                                                 int source_frame_number,
                                                 scoped_ptr<IPC::Message> msg,
                                                 bool* is_first) {
  base::AutoLock lock(lock_);
  GetSubQueue(policy)->QueueMessage(source_frame_number, msg.Pass(), is_first);
}

void FrameSwapMessageQueue::DidSwap(int source_frame_number) {
  base::AutoLock lock(lock_);

  visual_state_queue_->DrainMessages(source_frame_number,
                                     &next_drain_messages_);
}

void FrameSwapMessageQueue::DidNotSwap(int source_frame_number,
                                       cc::SwapPromise::DidNotSwapReason reason,
                                       ScopedVector<IPC::Message>* messages) {
  base::AutoLock lock(lock_);
  switch (reason) {
    case cc::SwapPromise::SWAP_FAILS:
    case cc::SwapPromise::COMMIT_NO_UPDATE:
      swap_queue_->DrainMessages(source_frame_number, messages);
    // fallthrough
    case cc::SwapPromise::COMMIT_FAILS:
      visual_state_queue_->DrainMessages(source_frame_number, messages);
      break;
    default:
      NOTREACHED();
  }
}

void FrameSwapMessageQueue::DrainMessages(
    ScopedVector<IPC::Message>* messages) {
  lock_.AssertAcquired();

  swap_queue_->DrainMessages(0, messages);
  messages->insert(messages->end(),
                   next_drain_messages_.begin(),
                   next_drain_messages_.end());
  next_drain_messages_.weak_clear();
}

scoped_ptr<FrameSwapMessageQueue::SendMessageScope>
FrameSwapMessageQueue::AcquireSendMessageScope() {
  return make_scoped_ptr(new SendMessageScopeImpl(&lock_))
      .PassAs<SendMessageScope>();
}

// static
void FrameSwapMessageQueue::TransferMessages(ScopedVector<IPC::Message>& source,
                                             vector<IPC::Message>* dest) {
  for (vector<IPC::Message*>::iterator i = source.begin(); i != source.end();
       ++i) {
    IPC::Message* m(*i);
    dest->push_back(*m);
    delete m;
  }
  source.weak_clear();
}

}  // namespace content
