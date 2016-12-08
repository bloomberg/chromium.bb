// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FIELD_TRIAL_RECORDER_H_
#define CHROME_BROWSER_FIELD_TRIAL_RECORDER_H_

#include "base/threading/thread_checker.h"
#include "chrome/common/field_trial_recorder.mojom.h"

class FieldTrialRecorder : public chrome::mojom::FieldTrialRecorder {
 public:
  FieldTrialRecorder();
  ~FieldTrialRecorder() override;

  static void Create(chrome::mojom::FieldTrialRecorderRequest request);

 private:
  // chrome::mojom::FieldTrialRecorder:
  void FieldTrialActivated(const std::string& trial_name) override;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialRecorder);
};

#endif  // CHROME_BROWSER_FIELD_TRIAL_RECORDER_H_
