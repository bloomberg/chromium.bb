// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_EXO_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_EXO_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace exo {
class Display;
class WMHelper;
namespace wayland {
class Server;
}
}

namespace arc {
class ArcNotificationSurfaceManager;
}

class ChromeBrowserMainExtraPartsExo : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsExo();
  ~ChromeBrowserMainExtraPartsExo() override;

  // Overridden from ChromeBrowserMainExtraParts:
  void PreProfileInit() override;
  void PostMainMessageLoopRun() override;

 private:
  std::unique_ptr<arc::ArcNotificationSurfaceManager>
      arc_notification_surface_manager_;
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<exo::Display> display_;
  std::unique_ptr<exo::wayland::Server> wayland_server_;
  class WaylandWatcher;
  std::unique_ptr<WaylandWatcher> wayland_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsExo);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_EXTRA_PARTS_EXO_H_
