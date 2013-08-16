// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_UTILITY_MAIN_THREAD_H_
#define CONTENT_UTILITY_UTILITY_MAIN_THREAD_H_

#include <string>

#include "base/threading/thread.h"
#include "content/common/content_export.h"

namespace content {

class ChildProcess;

class UtilityMainThread : public base::Thread {
 public:
  UtilityMainThread(const std::string& channel_id);
  virtual ~UtilityMainThread();
 private:

  // base::Thread implementation:
  virtual void Init() OVERRIDE;
  virtual void CleanUp() OVERRIDE;

  void InitInternal();

  std::string channel_id_;
  scoped_ptr<ChildProcess> child_process_;

  DISALLOW_COPY_AND_ASSIGN(UtilityMainThread);
};

CONTENT_EXPORT base::Thread* CreateUtilityMainThread(
    const std::string& channel_id);

}  // namespace content

#endif  // CONTENT_UTILITY_UTILITY_MAIN_THREAD_H_
