// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_

#include <set>

#include "base/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class MemoryMessageFilter;

class CONTENT_EXPORT MemoryPressureController {
 public:
  // These methods must be called on the IO thread.
  void OnMemoryMessageFilterAdded(MemoryMessageFilter* filter);
  void OnMemoryMessageFilterRemoved(MemoryMessageFilter* filter);

  // These methods can be called from any thread.
  void SetPressureNotificationsSuppressedInAllProcesses(bool suppressed);
  void SimulatePressureNotificationInAllProcesses(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // This method can be called from any thread.
  static MemoryPressureController* GetInstance();

 protected:
  virtual ~MemoryPressureController();

 private:
  friend struct base::DefaultSingletonTraits<MemoryPressureController>;

  MemoryPressureController();

  // Set of all memory message filters in the browser process. Always accessed
  // on the IO thread.
  typedef std::set<scoped_refptr<MemoryMessageFilter>> MemoryMessageFilterSet;
  MemoryMessageFilterSet memory_message_filters_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_
