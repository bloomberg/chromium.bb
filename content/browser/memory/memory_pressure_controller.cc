// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory/memory_pressure_controller.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "content/browser/memory/memory_message_filter.h"
#include "content/public/browser/browser_thread.h"

namespace content {

MemoryPressureController::MemoryPressureController() {}

MemoryPressureController::~MemoryPressureController() {}

void MemoryPressureController::OnMemoryMessageFilterAdded(
    MemoryMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Add the message filter to the set of all memory message filters and check
  // that it wasn't there beforehand.
  const bool success = memory_message_filters_.insert(filter).second;
  DCHECK(success);

  // There's no need to send a message to the child process if memory pressure
  // notifications are not suppressed.
  if (base::MemoryPressureListener::AreNotificationsSuppressed())
    filter->SendSetPressureNotificationsSuppressed(true);
}

void MemoryPressureController::OnMemoryMessageFilterRemoved(
    MemoryMessageFilter* filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Remove the message filter from the set of all memory message filters and
  // check that it was there beforehand.
  const bool success = memory_message_filters_.erase(filter) == 1u;
  DCHECK(success);
}

// static
MemoryPressureController* MemoryPressureController::GetInstance() {
  return base::Singleton<
      MemoryPressureController,
      base::LeakySingletonTraits<MemoryPressureController>>::get();
}

void MemoryPressureController::SetPressureNotificationsSuppressedInAllProcesses(
    bool suppressed) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Note that passing base::Unretained(this) is safe here because the
    // controller is a leaky singleton (i.e. it is never deleted).
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&MemoryPressureController::
                       SetPressureNotificationsSuppressedInAllProcesses,
                   base::Unretained(this), suppressed));
    return;
  }

  // Enable/disable suppressing memory notifications in the browser process.
  base::MemoryPressureListener::SetNotificationsSuppressed(suppressed);

  // Enable/disable suppressing memory notifications in all child processes.
  for (MemoryMessageFilterSet::iterator it = memory_message_filters_.begin();
       it != memory_message_filters_.end(); ++it) {
    it->get()->SendSetPressureNotificationsSuppressed(suppressed);
  }
}

}  // namespace content
