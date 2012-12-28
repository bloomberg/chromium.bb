// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_flash.h"

#include "ppapi/shared_impl/ppapi_permissions.h"

int32 kPepperFlashPermissions = ppapi::PERMISSION_DEV |
                                ppapi::PERMISSION_PRIVATE |
                                ppapi::PERMISSION_BYPASS_USER_GESTURE |
                                ppapi::PERMISSION_FLASH;

