// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_VSYNC_TIME_SOURCE_H_
#define CC_VSYNC_TIME_SOURCE_H_

#include "cc/cc_export.h"
#include "cc/time_source.h"

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
  virtual void setClient(TimeSourceClient* client) OVERRIDE;
  virtual void setTimebaseAndInterval(base::TimeTicks timebase,
                                      base::TimeDelta interval) OVERRIDE;
  virtual void setActive(bool active) OVERRIDE;
  virtual bool active() const OVERRIDE;
  virtual base::TimeTicks lastTickTime() OVERRIDE;
  virtual base::TimeTicks nextTickTime() OVERRIDE;

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

#endif  // CC_VSYNC_TIME_SOURCE_H_
