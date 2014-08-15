// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_
#define ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_

#include "athena/athena_export.h"
#include "base/memory/ref_counted.h"

namespace base {
class TaskRunner;
}

namespace athena {

class ATHENA_EXPORT SystemUI {
 public:
  // Creates and deletes the singleton object of the SystemUI implementation.
  static SystemUI* Create(scoped_refptr<base::TaskRunner> io_task_runner);
  static void Shutdown();

  virtual ~SystemUI() {}
};

}  // namespace athena

#endif  // ATHENA_SYSTEM_PUBLIC_SYSTEM_UI_H_
