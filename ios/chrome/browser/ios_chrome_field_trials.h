// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_
#define IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_

namespace base {
class CommandLine;
class Time;
}

// Sets up iOS-specific field trials.
void SetupFieldTrials(const base::CommandLine& command_line,
                      const base::Time& install_time);

#endif  // IOS_CHROME_BROWSER_IOS_CHROME_FIELD_TRIALS_H_
