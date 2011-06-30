// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MAC_LAUNCHD_H_
#define CHROME_BROWSER_MAC_LAUNCHD_H_
#pragma once

#include <string>

namespace launchd {

// Sends a signal to the job at |job_label|.
void SignalJob(const std::string& job_label, int signal);

}  // namespace launchd

#endif  // CHROME_BROWSER_MAC_LAUNCHD_H_
