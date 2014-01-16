// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_METRICS_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_METRICS_H_

namespace chromeos {
namespace first_run {

enum TutorialCompletion {
  // User left tutorial before finish.
  kTutorialNotFinished,
  // Tutorial was completed with "Got It" button.
  kTutorialCompletedWithGotIt,
  // Tutorial was completed with "Keep Exploring" button, i.e. Help App was
  // launched after tutorial.
  kTutorialCompletedWithKeepExploring,
  // Must be the last element.
  kTutorialCompletionSize
};

}  // namespace first_run
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_METRICS_H_
