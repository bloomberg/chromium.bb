// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_

#include "base/base_export.h"
#include "base/bind_internal.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"

namespace chromeos {

// A class that allows taking, saving and comparing screnshots while
// running tests.
class ScreenshotTester {
 public:
  ScreenshotTester();
  virtual ~ScreenshotTester();

  // Returns true if the screenshots should be taken and will be taken,
  // false otherwise. Also gets all the information from the command line
  // swithes.
  bool TryInitialize();

  // Does all the work that has been stated through switches:
  // updates golden screenshot or takes a new screenshot and compares it
  // with the golden one (this part is not implemented yet).
  void Run(const std::string& file_name);

 private:
  typedef scoped_refptr<base::RefCountedBytes> PNGFile;

  // Takes a screenshot and puts it to |screenshot_| field.
  void TakeScreenshot();

  // Saves |png_data| as a new golden screenshot for this test.
  void UpdateGoldenScreenshot(const std::string& file_name, PNGFile png_data);

  // Saves |png_data| as a current screenshot.
  void ReturnScreenshot(PNGFile png_data);

  // Path to the directory for golden screenshots.
  base::FilePath screenshot_dest_;

  // |run_loop_| and |run_loop_quitter_| are used to synchronize
  // with ui::GrabWindowSnapshotAsync.
  base::RunLoop run_loop_;
  base::Closure run_loop_quitter_;

  // Current screenshot.
  PNGFile screenshot_;

  // Is true when we running updating golden screenshots mode.
  bool update_golden_screenshot_;
  base::WeakPtrFactory<ScreenshotTester> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTester);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_
