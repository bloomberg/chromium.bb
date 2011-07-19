// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_LAUNCHD_H_
#define CHROME_BROWSER_MAC_LAUNCHD_H_
#pragma once

#include <sys/types.h>

#include <string>

namespace launchd {

// Returns the process ID for |job_label|, or -1 on error.
pid_t PIDForJob(const std::string& job_label);

}  // namespace launchd

#endif  // CHROME_BROWSER_MAC_LAUNCHD_H_
