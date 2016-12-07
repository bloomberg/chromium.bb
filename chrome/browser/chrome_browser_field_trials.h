// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/macros.h"

namespace base {
class FeatureList;
}

class ChromeBrowserFieldTrials {
 public:
  ChromeBrowserFieldTrials();
  ~ChromeBrowserFieldTrials();

  void SetupFieldTrials();

  // Create field trials that will control feature list features. This should be
  // called during the same timing window as
  // FeatureList::AssociateReportingFieldTrial. |has_seed| indicates that the
  // variations service used a seed to create field trials. This can be used to
  // prevent associating a field trial with a feature that you expect to be
  // controlled by the variations seed.
  void SetupFeatureControllingFieldTrials(bool has_seed,
                                          base::FeatureList* feature_list);

 private:
  // Instantiates dynamic trials by querying their state, to ensure they get
  // reported as used.
  void InstantiateDynamicTrials();

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserFieldTrials);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
