// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_
#define CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"

namespace content {

class BrowserChildProcessHost;
class MemoryMessageFilter;
class RenderProcessHost;

class CONTENT_EXPORT MemoryPressureController {
 public:
  // These methods must be called on the IO thread.
  void OnMemoryMessageFilterAdded(MemoryMessageFilter* filter);
  void OnMemoryMessageFilterRemoved(MemoryMessageFilter* filter);

  // These methods can be called from any thread.
  void SetPressureNotificationsSuppressedInAllProcesses(bool suppressed);
  void SimulatePressureNotificationInAllProcesses(
      base::MemoryPressureListener::MemoryPressureLevel level);
  void SendPressureNotification(
      const BrowserChildProcessHost* child_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);
  void SendPressureNotification(
      const RenderProcessHost* render_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

  // This method can be called from any thread.
  static MemoryPressureController* GetInstance();

 protected:
  virtual ~MemoryPressureController();

 private:
  friend struct base::DefaultSingletonTraits<MemoryPressureController>;

  MemoryPressureController();

  // Implementation of the various SendPressureNotification methods.
  void SendPressureNotificationImpl(
      const void* child_process_host,
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Map from untyped process host pointers to the associated memory message
  // filters in the browser process. Always accessed on the IO thread.
  typedef std::map<const void*, scoped_refptr<MemoryMessageFilter>>
      MemoryMessageFilterMap;
  MemoryMessageFilterMap memory_message_filters_;

  DISALLOW_COPY_AND_ASSIGN(MemoryPressureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_MEMORY_PRESSURE_CONTROLLER_H_
