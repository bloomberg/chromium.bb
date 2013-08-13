// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_
#define CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_

#include "base/basictypes.h"
#include "content/public/common/main_function_params.h"

class NaClMainPlatformDelegate {
 public:
  explicit NaClMainPlatformDelegate(
      const content::MainFunctionParams& parameters);
  ~NaClMainPlatformDelegate();

  // Initiate Lockdown.
  void EnableSandbox();

 private:
  const content::MainFunctionParams& parameters_;

  DISALLOW_COPY_AND_ASSIGN(NaClMainPlatformDelegate);
};

#endif  // CHROME_NACL_NACL_MAIN_PLATFORM_DELEGATE_H_
