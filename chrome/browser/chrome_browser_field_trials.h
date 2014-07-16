// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/basictypes.h"
#include "base/command_line.h"

class PrefService;

namespace base {
class Time;
}

class ChromeBrowserFieldTrials {
 public:
  explicit ChromeBrowserFieldTrials(const base::CommandLine& command_line);
  ~ChromeBrowserFieldTrials();

  // Called by the browser main sequence to set up Field Trials for this client.
  // |local_state| is used to set browser-wide properties.
  void SetupFieldTrials(const base::Time& install_time,
                        PrefService* local_state);

 private:
  // Instantiates dynamic trials by querying their state, to ensure they get
  // reported as used.
  void InstantiateDynamicTrials();

  const base::CommandLine& parsed_command_line_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserFieldTrials);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
