// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/message_loop_factory_impl.h"

namespace media {

MessageLoopFactoryImpl::MessageLoopFactoryImpl() {}

MessageLoopFactoryImpl::~MessageLoopFactoryImpl() {
  base::AutoLock auto_lock(lock_);

  for (ThreadMap::iterator iter = thread_map_.begin();
       iter != thread_map_.end();
       ++iter) {
    base::Thread* thread = (*iter).second;

    if (thread) {
      thread->Stop();
      delete thread;
    }
  }
  thread_map_.clear();
}

// MessageLoopFactory methods.
MessageLoop* MessageLoopFactoryImpl::GetMessageLoop(const std::string& name) {
  if (name.empty()) {
    return NULL;
  }

  base::AutoLock auto_lock(lock_);

  ThreadMap::iterator it = thread_map_.find(name);
  if (it != thread_map_.end())
    return (*it).second->message_loop();

  scoped_ptr<base::Thread> thread(new base::Thread(name.c_str()));

  if (thread->Start()) {
    MessageLoop* message_loop = thread->message_loop();
    thread_map_[name] = thread.release();
    return message_loop;
  }

  LOG(ERROR) << "Failed to start '" << name << "' thread!";
  return NULL;
}

}  // namespace media
