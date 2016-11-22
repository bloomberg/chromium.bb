// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

class ChromeLauncherControllerMus;
class ImmersiveContextMus;
class ImmersiveHandlerFactoryMus;
class SystemTrayClient;
class VpnListForwarder;

class ChromeBrowserMainExtraPartsAsh : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsAsh();
  ~ChromeBrowserMainExtraPartsAsh() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PostProfileInit() override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<ChromeLauncherControllerMus> chrome_launcher_controller_mus_;
  std::unique_ptr<ImmersiveHandlerFactoryMus> immersive_handler_factory_;
  std::unique_ptr<ImmersiveContextMus> immersive_context_;
  std::unique_ptr<SystemTrayClient> system_tray_client_;
  std::unique_ptr<VpnListForwarder> vpn_list_forwarder_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_CHROME_BROWSER_MAIN_EXTRA_PARTS_ASH_H_
