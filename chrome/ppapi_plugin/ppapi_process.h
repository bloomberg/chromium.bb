// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PPAPI_PLUGIN_PPAPI_PROCESS_H_
#define CHROME_PPAPI_PLUGIN_PPAPI_PROCESS_H_
#pragma once

#include "content/common/child_process.h"

class PpapiProcess : public ChildProcess {
 public:
  PpapiProcess();
  ~PpapiProcess();

 private:
  DISALLOW_COPY_AND_ASSIGN(PpapiProcess);
};

#endif  // CHROME_PPAPI_PLUGIN_PPAPI_PROCESS_H_
