// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FIRST_RUN_GOODIES_DISPLAYER_H_
#define CHROME_BROWSER_CHROMEOS_FIRST_RUN_GOODIES_DISPLAYER_H_

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"

namespace chromeos {
namespace first_run {

// Handles display of OOBE Goodies page on first display of browser window on
// new Chromebooks.
class GoodiesDisplayer : public chrome::BrowserListObserver {
 public:
  GoodiesDisplayer();
  ~GoodiesDisplayer() override {}

  static void Init();

 private:
  // Overridden from chrome::BrowserListObserver.
  void OnBrowserSetLastActive(Browser* browser) override;

  DISALLOW_COPY_AND_ASSIGN(GoodiesDisplayer);
};

}  // namespace first_run
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FIRST_RUN_GOODIES_DISPLAYER_H_
