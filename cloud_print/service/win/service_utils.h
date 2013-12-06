// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SERVICE_UTILS_H_
#define CLOUD_PRINT_SERVICE_SERVICE_UTILS_H_

class CommandLine;

#include "base/strings/string16.h"

base::string16 ReplaceLocalHostInName(const base::string16& user_name);
base::string16 GetCurrentUserName();
void CopyChromeSwitchesFromCurrentProcess(CommandLine* destination);

#endif  // CLOUD_PRINT_SERVICE_SERVICE_UTILS_H_

