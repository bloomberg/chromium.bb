// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A mock of talk_base::Task.

#ifndef JINGLE_NOTIFIER_MOCK_TASK_H_
#define JINGLE_NOTIFIER_MOCK_TASK_H_
#pragma once

#include "talk/base/task.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace notifier {

class MockTask : public talk_base::Task {
 public:
  MockTask(TaskParent* parent);

  virtual ~MockTask();

  MOCK_METHOD0(ProcessStart, int());
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_MOCK_TASK_H_
