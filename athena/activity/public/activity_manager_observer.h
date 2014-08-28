// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_OBSERVER_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_OBSERVER_H_

#include "athena/athena_export.h"

namespace athena {

class Activity;

class ATHENA_EXPORT ActivityManagerObserver {
 public:
  virtual ~ActivityManagerObserver() {}

  // Called after an |activity| got created.
  virtual void OnActivityStarted(Activity* activity) = 0;

  // Called before an |activity| gets destroyed.
  virtual void OnActivityEnding(Activity* activity) = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_OBSERVER_H_
