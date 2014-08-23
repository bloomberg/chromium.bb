// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/conditional_work_item_list.h"

#include "base/files/file_util.h"
#include "base/logging.h"

ConditionalWorkItemList::ConditionalWorkItemList(Condition* condition)
    : condition_(condition) {
}

ConditionalWorkItemList::~ConditionalWorkItemList() {}

bool ConditionalWorkItemList::Do() {
  VLOG(1) << "Evaluating " << log_message_ << " condition...";
  if (condition_.get() && condition_->ShouldRun()) {
    VLOG(1) << "Beginning conditional work item list";
    return WorkItemList::Do();
  }
  VLOG(1) << "No work to do in condition work item list "
          << log_message_;
  return true;
}

void ConditionalWorkItemList::Rollback() {
  VLOG(1) << "Rolling back conditional list " << log_message_;
  WorkItemList::Rollback();
}

// Pre-defined conditions:
//------------------------------------------------------------------------------
bool ConditionRunIfFileExists::ShouldRun() const {
  return base::PathExists(key_path_);
}

bool Not::ShouldRun() const {
  return !original_condition_->ShouldRun();
}

