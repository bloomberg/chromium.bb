// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_

#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/profiler_controller.h"

namespace content {

// ProfilerController's implementation.
class CONTENT_EXPORT ProfilerControllerImpl : public ProfilerController {
 public:
  static ProfilerControllerImpl* GetInstance();

  // Normally instantiated when the child process is launched. Only one instance
  // should be created per process.
  ProfilerControllerImpl();
  virtual ~ProfilerControllerImpl();

  // Send number of pending processes to subscriber_. |end| is set to true if it
  // is the last time. This is called on UI thread.
  void OnPendingProcesses(int sequence_number, int pending_processes, bool end);

  // Send profiler_data back to subscriber_. subscriber_ assumes the ownership
  // of profiler_data. This is called on UI thread.
  void OnProfilerDataCollected(int sequence_number,
                               base::DictionaryValue* profiler_data);

  // ProfilerController implementation:
  virtual void Register(ProfilerSubscriber* subscriber) OVERRIDE;
  virtual void Unregister(ProfilerSubscriber* subscriber) OVERRIDE;
  virtual void GetProfilerData(int sequence_number) OVERRIDE;
  virtual void SetProfilerStatus(bool enable) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ProfilerControllerImpl>;

  // Contact child processes and get their profiler data.
  void GetProfilerDataFromChildProcesses(int sequence_number);

  // Contact child processes and set profiler status to |enable|.
  void SetProfilerStatusInChildProcesses(bool enable);

  ProfilerSubscriber* subscriber_;

  DISALLOW_COPY_AND_ASSIGN(ProfilerControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PROFILER_CONTROLLER_IMPL_H_

