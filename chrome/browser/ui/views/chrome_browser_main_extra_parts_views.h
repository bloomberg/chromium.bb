// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace ui {
class InputDeviceClient;
}

namespace views {
class ViewsDelegate;
}

#if defined(USE_AURA)
namespace views {
class MusClient;
}
namespace wm {
class WMState;
}
#endif

class ChromeBrowserMainExtraPartsViews : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsViews();
  ~ChromeBrowserMainExtraPartsViews() override;

#if defined(USE_AURA)
  wm::WMState* wm_state() { return wm_state_.get(); }
#endif

  // Overridden from ChromeBrowserMainExtraParts:
  void ToolkitInitialized() override;
  void PreCreateThreads() override;
  void PreProfileInit() override;
  void ServiceManagerConnectionStarted(
      content::ServiceManagerConnection* connection) override;

 private:
  std::unique_ptr<views::ViewsDelegate> views_delegate_;

#if defined(USE_AURA)
  std::unique_ptr<wm::WMState> wm_state_;
  std::unique_ptr<views::MusClient> mus_client_;

  // Subscribes to updates about input-devices.
  std::unique_ptr<ui::InputDeviceClient> input_device_client_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CHROME_BROWSER_MAIN_EXTRA_PARTS_VIEWS_H_
