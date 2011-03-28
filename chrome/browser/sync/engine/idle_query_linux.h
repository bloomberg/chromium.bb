// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_IDLE_QUERY_LINUX_H_
#define CHROME_BROWSER_SYNC_ENGINE_IDLE_QUERY_LINUX_H_
#pragma once

#include "base/memory/scoped_ptr.h"

namespace browser_sync {

class IdleData;

class IdleQueryLinux {
 public:
  IdleQueryLinux();
  ~IdleQueryLinux();
  int IdleTime();

 private:
  scoped_ptr<IdleData> idle_data_;
};

}  // namespace browser_sync
#endif  // CHROME_BROWSER_SYNC_ENGINE_IDLE_QUERY_LINUX_H_
