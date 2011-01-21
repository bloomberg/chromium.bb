// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MESSAGE_LOOP_FACTORY_IMPL_H_
#define MEDIA_BASE_MESSAGE_LOOP_FACTORY_IMPL_H_

#include <map>
#include <string>

#include "base/threading/thread.h"
#include "media/base/message_loop_factory.h"

namespace media {

class MessageLoopFactoryImpl : public MessageLoopFactory {
 public:
  MessageLoopFactoryImpl();

  // MessageLoopFactory methods.
  virtual MessageLoop* GetMessageLoop(const std::string& name);

 protected:
  virtual ~MessageLoopFactoryImpl();

 private:
  // Lock used to serialize access for the following data members.
  base::Lock lock_;

  typedef std::map<std::string, base::Thread*> ThreadMap;
  ThreadMap thread_map_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopFactoryImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_MESSAGE_LOOP_FACTORY_IMPL_H_
