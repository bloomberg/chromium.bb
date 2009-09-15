// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cocoa/task_manager_mac.h"

#include <algorithm>
#include <vector>

#include "base/mac_util.h"

////////////////////////////////////////////////////////////////////////////////
// TaskManagerWindowController implementation:

@implementation TaskManagerWindowController

- (id)init {
  NSString* nibpath = [mac_util::MainAppBundle()
                        pathForResource:@"TaskManager"
                                 ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibpath owner:self])) {
    [[self window] makeKeyAndOrderFront:self];
  }
  return self;
}

@end

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac implementation:

TaskManagerMac::TaskManagerMac()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()) {
  window_controller_.reset([[TaskManagerWindowController alloc] init]);
}

// static
TaskManagerMac* TaskManagerMac::instance_ = NULL;

TaskManagerMac::~TaskManagerMac() {
  task_manager_->OnWindowClosed();
  model_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, TaskManagerModelObserver implementation:

void TaskManagerMac::OnModelChanged() {
}

void TaskManagerMac::OnItemsChanged(int start, int length) {
}

void TaskManagerMac::OnItemsAdded(int start, int length) {
}

void TaskManagerMac::OnItemsRemoved(int start, int length) {
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerMac, public:

// static
void TaskManagerMac::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    [[instance_->window_controller_ window]
        makeKeyAndOrderFront:instance_->window_controller_];
  } else {
    instance_ = new TaskManagerMac;
    instance_->model_->StartUpdating();
  }
}
