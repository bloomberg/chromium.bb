// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_LOOP_H_
#define CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_LOOP_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"

namespace base {
class MessageLoopProxy;
class Thread;
}

namespace chromecast {
namespace media {

class CmaMessageLoop {
 public:
  static scoped_refptr<base::MessageLoopProxy> GetMessageLoopProxy();

 private:
  friend struct DefaultSingletonTraits<CmaMessageLoop>;
  friend class Singleton<CmaMessageLoop>;

  static CmaMessageLoop* GetInstance();

  CmaMessageLoop();
  ~CmaMessageLoop();

  scoped_ptr<base::Thread> thread_;

  DISALLOW_COPY_AND_ASSIGN(CmaMessageLoop);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_MEDIA_CMA_MESSAGE_LOOP_H_
