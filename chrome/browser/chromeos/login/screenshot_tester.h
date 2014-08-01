// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_

#include <string>

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
  // with the golden one. |test_name| is the name of the test from which
  // we run this method.
  void Run(const std::string& test_name);

 private:
  typedef scoped_refptr<base::RefCountedBytes> PNGFile;

  // Takes a screenshot and returns it.
  PNGFile TakeScreenshot();

  // Saves |png_data| as a new golden screenshot for test |test_name_|.
  void UpdateGoldenScreenshot(PNGFile png_data);

  // Saves an image |png_data|, assuming it is a .png file.
  // Returns true if image was saved successfully.
  bool SaveImage(const std::string& file_name,
                 const base::FilePath& screenshot_dir,
                 PNGFile png_data);

  // Saves |png_data| as a current screenshot.
  void ReturnScreenshot(const PNGFile& screenshot, PNGFile png_data);

  // Loads golden screenshot from the disk. Fails if there is no
  // golden screenshot for test |test_name_|.
  PNGFile LoadGoldenScreenshot();

  // Compares two given screenshots and saves |sample|
  // and difference between |sample| and |model|, if they differ in any pixel.
  void CompareScreenshots(PNGFile model, PNGFile sample);

  // Name of the test from which Run() method has been called.
  // Used for generating names for screenshot files.
  std::string test_name_;

  // Path to the directory for golden screenshots.
  base::FilePath golden_screenshots_dir_;

  // Path to the directory where screenshots that failed comparing
  // and difference between them and golden ones will be stored.
  base::FilePath artifacts_dir_;

  // |run_loop_| and |run_loop_quitter_| are used to synchronize
  // with ui::GrabWindowSnapshotAsync.
  base::RunLoop run_loop_;
  base::Closure run_loop_quitter_;

  // Is true when we're in test mode:
  // comparing golden screenshots and current ones.
  bool test_mode_;

  base::WeakPtrFactory<ScreenshotTester> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenshotTester);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENSHOT_TESTER_H_
