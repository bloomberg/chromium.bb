// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_MEDIA_MESSAGE_LOOP_H_
#define CHROMECAST_MEDIA_BASE_MEDIA_MESSAGE_LOOP_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"

namespace base {
class SingleThreadTaskRunner;
class Thread;
}

namespace chromecast {
namespace media {

class MediaMessageLoop {
 public:
  static scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner();

  static MediaMessageLoop* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<MediaMessageLoop>;

  MediaMessageLoop();
  ~MediaMessageLoop();

  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(MediaMessageLoop);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_MEDIA_MESSAGE_LOOP_H_
