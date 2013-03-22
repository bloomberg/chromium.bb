// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PROCESS_TYPE_H_
#define CHROME_COMMON_CHROME_PROCESS_TYPE_H_

#include "content/public/common/process_type.h"

// Defines the process types that are custom to chrome (i.e. as opposed to the
// ones that content knows about).
enum ChromeProcessType {
  PROCESS_TYPE_PROFILE_IMPORT = content::PROCESS_TYPE_CONTENT_END,
  PROCESS_TYPE_NACL_LOADER,
  PROCESS_TYPE_NACL_BROKER,
};

#endif  // CHROME_COMMON_CHROME_PROCESS_TYPE_H_
