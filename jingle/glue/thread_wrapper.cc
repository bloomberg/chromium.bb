// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/thread_wrapper.h"

namespace jingle_glue {

// static
void JingleThreadWrapper::EnsureForCurrentThread() {
  talk_base::Thread* current_thread = talk_base::Thread::Current();
  // If JingleThreadWrapper already exists for the current thread then
  // it is returned by talk_base::Thread::Current().
  // talk_base::Thread::Current() may also return non-null value for
  // the main thread because talk_base::ThreadManager creates
  // talk_base::Thread object for it. IsOwned() allows to distinguish
  // talk_base::Thread object created by talk_base::ThreadManager from
  // other talk_base::Thread objects. Because talk_base::Thread
  // objects should never created by chromium code, we can assume that
  // if talk_base::Thread::Current() returns non-null value and it
  // isn't the object created by talk_base::ThreadManager then
  // JingleThreadWrapper already exists for the current thread.
  if (current_thread == NULL || !current_thread->IsOwned()) {
    new JingleThreadWrapper(MessageLoop::current());
  }
}

JingleThreadWrapper::JingleThreadWrapper(MessageLoop* message_loop)
    : message_loop_(message_loop) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  talk_base::ThreadManager::SetCurrent(this);
  talk_base::MessageQueueManager::Instance()->Add(this);
  message_loop_->AddDestructionObserver(this);
}

JingleThreadWrapper::~JingleThreadWrapper() {
}

void JingleThreadWrapper::WillDestroyCurrentMessageLoop() {
  talk_base::ThreadManager::SetCurrent(NULL);
  talk_base::MessageQueueManager::Instance()->Remove(this);
  message_loop_->RemoveDestructionObserver(this);
  delete this;
}

void JingleThreadWrapper::Post(
    talk_base::MessageHandler* handler, uint32 message_id,
    talk_base::MessageData* data, bool time_sensitive) {
  PostTaskInternal(0, handler, message_id, data);
}

void JingleThreadWrapper::PostDelayed(
    int delay_ms, talk_base::MessageHandler* handler,
    uint32 message_id, talk_base::MessageData* data) {
  PostTaskInternal(delay_ms, handler, message_id, data);
}

void JingleThreadWrapper::Clear(talk_base::MessageHandler* handler, uint32 id,
                                talk_base::MessageList* removed) {
  base::AutoLock auto_lock(lock_);

  for (MessagesQueue::iterator it = messages_.begin();
       it != messages_.end();) {
    if (it->second.Match(handler, id)) {
      if (removed) {
        removed->push_back(it->second);
      } else {
        delete it->second.pdata;
      }
      MessagesQueue::iterator next = it;
      ++next;
      messages_.erase(it);
      it = next;
    } else {
      ++it;
    }
  }
}

void JingleThreadWrapper::PostTaskInternal(
    int delay_ms, talk_base::MessageHandler* handler,
    uint32 message_id, talk_base::MessageData* data) {
  int task_id;
  talk_base::Message message;
  message.phandler = handler;
  message.message_id = message_id;
  message.pdata = data;
  {
    base::AutoLock auto_lock(lock_);
    task_id = ++last_task_id_;
    messages_.insert(std::pair<int, talk_base::Message>(task_id, message));
  }

  if (delay_ms <= 0) {
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &JingleThreadWrapper::RunTask, task_id));
  } else {
    message_loop_->PostDelayedTask(
        FROM_HERE,
        NewRunnableMethod(this, &JingleThreadWrapper::RunTask, task_id),
        delay_ms);
  }
}

void JingleThreadWrapper::RunTask(int task_id) {
  bool have_message = false;
  talk_base::Message message;
  {
    base::AutoLock auto_lock(lock_);
    MessagesQueue::iterator it = messages_.find(task_id);
    if (it != messages_.end()) {
      have_message = true;
      message = it->second;
      messages_.erase(it);
    }
  }

  if (have_message)
    message.phandler->OnMessage(&message);
}

// All methods below are marked as not reached. See comments in the
// header for more details.
void JingleThreadWrapper::Quit() {
  NOTREACHED();
}

bool JingleThreadWrapper::IsQuitting() {
  NOTREACHED();
  return false;
}

void JingleThreadWrapper::Restart() {
  NOTREACHED();
}

bool JingleThreadWrapper::Get(talk_base::Message*, int, bool) {
  NOTREACHED();
  return false;
}

bool JingleThreadWrapper::Peek(talk_base::Message*, int) {
  NOTREACHED();
  return false;
}

void JingleThreadWrapper::PostAt(uint32, talk_base::MessageHandler*,
                                 uint32, talk_base::MessageData*) {
  NOTREACHED();
}

void JingleThreadWrapper::Dispatch(talk_base::Message* msg) {
  NOTREACHED();
}

void JingleThreadWrapper::ReceiveSends() {
  NOTREACHED();
}

int JingleThreadWrapper::GetDelay() {
  NOTREACHED();
  return 0;
}

void JingleThreadWrapper::Stop() {
  NOTREACHED();
}

void JingleThreadWrapper::Run() {
  NOTREACHED();
}

}  // namespace jingle_glue
