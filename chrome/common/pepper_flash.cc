// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_flash.h"

#include "ppapi/shared_impl/ppapi_permissions.h"

bool ConductingPepperFlashFieldTrial() {
#if defined(OS_WIN)
  return true;
#elif defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

bool IsPepperFlashEnabledByDefault() {
#if defined(USE_AURA)
  // Pepper Flash is required for Aura (on any OS).
  return true;
#elif defined(OS_WIN)
  return true;
#elif defined(OS_LINUX)
  // For Linux, always try to use it (availability is checked elsewhere).
  return true;
#elif defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

int32 kPepperFlashPermissions = ppapi::PERMISSION_DEV |
                                ppapi::PERMISSION_PRIVATE |
                                ppapi::PERMISSION_BYPASS_USER_GESTURE |
                                ppapi::PERMISSION_FLASH;


