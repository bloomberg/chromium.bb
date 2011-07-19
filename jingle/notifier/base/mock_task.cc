// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/mock_task.h"

namespace notifier {

MockTask::MockTask(TaskParent* parent) : talk_base::Task(parent) {}

MockTask::~MockTask() {}

}  // namespace notifier
