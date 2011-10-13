// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_

#include "base/memory/ref_counted.h"
#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chrome_browser_main_gtk.h"
typedef ChromeBrowserMainPartsGtk ChromeBrowserMainPartsBase;
#else
#include "chrome/browser/chrome_browser_main_posix.h"
#include "chrome/browser/chrome_browser_main_x11.h"
typedef ChromeBrowserMainPartsPosix ChromeBrowserMainPartsBase;
#endif

namespace sensors {
class SensorsSourceChromeos;
}  // namespace sensors

class ChromeBrowserMainPartsChromeos : public ChromeBrowserMainPartsBase {
 public:
  explicit ChromeBrowserMainPartsChromeos(const MainFunctionParams& parameters);

  virtual ~ChromeBrowserMainPartsChromeos();

  virtual void PreEarlyInitialization() OVERRIDE;
  virtual void PreMainMessageLoopStart() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual void PostMainMessageLoopStart() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsChromeos);
};

#endif  // CHROME_BROWSER_CHROMEOS_CHROME_BROWSER_MAIN_CHROMEOS_H_
