// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PAI_STARTER_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PAI_STARTER_H_

#include "base/macros.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"

namespace content {
class BrowserContext;
}

namespace arc {

// Helper class that starts Play Auto Install flow when all conditions are met:
// The Play Store app is ready and there is no lock for PAI flow.
class ArcPaiStarter : public ArcAppListPrefs::Observer {
 public:
  explicit ArcPaiStarter(content::BrowserContext* context);
  ~ArcPaiStarter() override;

  // Locks PAI to be run on the Play Store app is ready.
  void AcquireLock();

  // Unlocks PAI to be run on the Play Store app is ready. If the Play Store app
  // is ready at this moment then PAI is started immediately.
  void ReleaseLock();

  bool started() const { return started_; }

 private:
  void MaybeStartPai();

  // ArcAppListPrefs::Observer:
  void OnAppReadyChanged(const std::string& app_id, bool ready) override;

  content::BrowserContext* const context_;
  bool locked_ = false;
  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(ArcPaiStarter);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PAI_STARTER_H_
