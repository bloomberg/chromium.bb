// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_

#include <string>

#include "athena/athena_export.h"

namespace athena {

class ActivityViewModel;

class ATHENA_EXPORT Activity {
 public:
  virtual ~Activity();

  // The Activity retains ownership of the returned view-model.
  virtual ActivityViewModel* GetActivityViewModel() = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_H_
