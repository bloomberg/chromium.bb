// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_VSYNC_TIME_SOURCE_H_
#define CC_SCHEDULER_VSYNC_TIME_SOURCE_H_

#include "cc/base/cc_export.h"
#include "cc/scheduler/time_source.h"

namespace cc {

class CC_EXPORT VSyncClient {
 public:
  virtual void DidVSync(base::TimeTicks frame_time) = 0;

 protected:
  virtual ~VSyncClient() {}
};

class VSyncProvider {
 public:
  // Request to be notified of future vsync events. The notifications will be
  // delivered until they are disabled by calling this function with a null
  // client.
  virtual void RequestVSyncNotification(VSyncClient* client) = 0;

 protected:
  virtual ~VSyncProvider() {}
};

// This timer implements a time source that is explicitly triggered by an
// external vsync signal.
class CC_EXPORT VSyncTimeSource : public TimeSource, public VSyncClient {
 public:
  static scoped_refptr<VSyncTimeSource> create(VSyncProvider* vsync_provider);

  // TimeSource implementation
  virtual void SetClient(TimeSourceClient* client) OVERRIDE;
  virtual void SetTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval) OVERRIDE;
  virtual void SetActive(bool active) OVERRIDE;
  virtual bool Active() const OVERRIDE;
  virtual base::TimeTicks LastTickTime() OVERRIDE;
  virtual base::TimeTicks NextTickTime() OVERRIDE;

  // VSyncClient implementation
  virtual void DidVSync(base::TimeTicks frame_time) OVERRIDE;

 protected:
  explicit VSyncTimeSource(VSyncProvider* vsync_provider);
  virtual ~VSyncTimeSource();

  base::TimeTicks last_tick_time_;
  base::TimeDelta interval_;
  bool active_;
  bool notification_requested_;

  VSyncProvider* vsync_provider_;
  TimeSourceClient* client_;

  DISALLOW_COPY_AND_ASSIGN(VSyncTimeSource);
};

}  // namespace cc

#endif  // CC_SCHEDULER_VSYNC_TIME_SOURCE_H_
