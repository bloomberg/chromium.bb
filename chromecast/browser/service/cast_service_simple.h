// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
#define CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_

#include <memory>

#include "base/macros.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/service/cast_service.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace chromecast {
class CastWindowManager;

namespace shell {

class CastServiceSimple : public CastService,
                          public CastContentWindow::Delegate {
 public:
  CastServiceSimple(content::BrowserContext* browser_context,
                    PrefService* pref_service,
                    CastWindowManager* window_manager);
  ~CastServiceSimple() override;

 protected:
  // CastService implementation:
  void InitializeInternal() override;
  void FinalizeInternal() override;
  void StartInternal() override;
  void StopInternal() override;

  // CastContentWindow::Delegate implementation:
  void OnWindowDestroyed() override;
  void OnKeyEvent(const ui::KeyEvent& key_event) override;

 private:
  CastWindowManager* const window_manager_;
  std::unique_ptr<CastContentWindow> window_;
  std::unique_ptr<content::WebContents> web_contents_;
  GURL startup_url_;

  DISALLOW_COPY_AND_ASSIGN(CastServiceSimple);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_SERVICE_CAST_SERVICE_SIMPLE_H_
