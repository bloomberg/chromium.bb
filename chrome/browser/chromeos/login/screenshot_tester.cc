// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screenshot_tester.h"

#include "ash/shell.h"
#include "base/base_export.h"
#include "base/bind_internal.h"
#include "base/command_line.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/browser/browser_thread.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace chromeos {

ScreenshotTester::ScreenshotTester() : weak_factory_(this) {
}

ScreenshotTester::~ScreenshotTester() {
}

bool ScreenshotTester::TryInitialize() {
  CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kEnableScreenshotTesting))
    return false;
  if (!command_line.HasSwitch(::switches::kEnablePixelOutputInTests) ||
      !command_line.HasSwitch(::switches::kUIEnableImplSidePainting)) {
    // TODO(elizavetai): make turning on --enable-pixel-output-in-tests
    // and --ui-enable-impl-side-painting automatical.
    LOG(ERROR) << "--enable-pixel-output-in-tests and "
               << "--ui-enable-impl-side-painting are required to take "
               << "screenshots";
    return false;
  }
  if (!command_line.HasSwitch(switches::kScreenshotDestinationDir)) {
    LOG(ERROR) << "No destination for screenshots specified";
    return false;
  }
  screenshot_dest_ = command_line.GetSwitchValuePath(
      chromeos::switches::kScreenshotDestinationDir);
  return true;
}

void ScreenshotTester::Run(const std::string& file_name) {
  TakeScreenshot();
  PNGFile current_screenshot = screenshot_;
  UpdateGoldenScreenshot(file_name, current_screenshot);
}

void ScreenshotTester::UpdateGoldenScreenshot(const std::string& file_name,
                                              PNGFile png_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!png_data) {
    LOG(ERROR) << "Can't take a screenshot";
    return;
  }
  base::FilePath golden_screenshot_path =
      screenshot_dest_.AppendASCII(file_name + ".png");
  if (!base::CreateDirectory(golden_screenshot_path.DirName())) {
    LOG(ERROR) << "Can't create a directory ";
    return;
  }
  if (static_cast<size_t>(
          base::WriteFile(golden_screenshot_path,
                          reinterpret_cast<char*>(&(png_data->data()[0])),
                          png_data->size())) != png_data->size()) {
    LOG(ERROR) << "Can't save screenshot";
  }
  VLOG(0) << "Golden screenshot updated successfully";
}

void ScreenshotTester::ReturnScreenshot(PNGFile png_data) {
  // TODO(elizavetai): rewrite this function so that TakeScreenshot
  // could return a |PNGFile| -- current screenshot.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  screenshot_ = png_data;
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE, run_loop_quitter_);
}

void ScreenshotTester::TakeScreenshot() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aura::Window* primary_window = ash::Shell::GetPrimaryRootWindow();
  gfx::Rect rect = primary_window->bounds();
  ui::GrabWindowSnapshotAsync(primary_window,
                              rect,
                              content::BrowserThread::GetBlockingPool(),
                              base::Bind(&ScreenshotTester::ReturnScreenshot,
                                         weak_factory_.GetWeakPtr()));
  base::RunLoop run_loop;
  run_loop_quitter_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace chromeos
