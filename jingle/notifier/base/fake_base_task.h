// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A simple wrapper around invalidation::InvalidationClient that
// handles all the startup/shutdown details and hookups.

#ifndef JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_
#define JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_
#pragma once

#include "base/basictypes.h"
#include "base/weak_ptr.h"
#include "jingle/notifier/base/task_pump.h"

namespace talk_base {
class Task;
}  // namespace talk_base

namespace notifier {

// This class expects a message loop to exist on the current thread.
class FakeBaseTask {
 public:
  FakeBaseTask();

  base::WeakPtr<talk_base::Task> AsWeakPtr();

 private:
  notifier::TaskPump task_pump_;
  base::WeakPtr<talk_base::Task> base_task_;

  DISALLOW_COPY_AND_ASSIGN(FakeBaseTask);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_FAKE_XMPP_CLIENT_H_
