// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_MEDIA_TASK_RUNNER_H_
#define CHROMECAST_MEDIA_CMA_BASE_MEDIA_TASK_RUNNER_H_

#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace chromecast {
namespace media {

class MediaTaskRunner
    : public base::RefCountedThreadSafe<MediaTaskRunner> {
 public:
  MediaTaskRunner();

  // Post a task with the given media |timestamp|. If |timestamp| is equal to
  // |kNoTimestamp|, the task is scheduled right away.
  // How the media timestamp is used to schedule the task is an implementation
  // detail of derived classes.
  // Returns true if the task may be run at some point in the future, and false
  // if the task definitely will not be run.
  virtual bool PostMediaTask(const base::Location& from_here,
                             const base::Closure& task,
                             base::TimeDelta timestamp) = 0;

 protected:
  virtual ~MediaTaskRunner();
  friend class base::RefCountedThreadSafe<MediaTaskRunner>;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaTaskRunner);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_MEDIA_TASK_RUNNER_H_
