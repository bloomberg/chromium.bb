// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_

#include "base/macros.h"
#include "components/arc/common/screen_capture.mojom.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

class ArcScreenCaptureBridge : public KeyedService,
                               public mojom::ScreenCaptureHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcScreenCaptureBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcScreenCaptureBridge(content::BrowserContext* context,
                         ArcBridgeService* bridge_service);
  ~ArcScreenCaptureBridge() override;

  // mojom::ScreenCaptureHost overrides:
  void OpenSession(mojom::ScreenCaptureSessionNotifierPtr notifier,
                   const std::string& package_name,
                   const gfx::Size& size,
                   OpenSessionCallback callback) override;
  void RequestPermission(const std::string& display_name,
                         const std::string& package_name,
                         RequestPermissionCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcScreenCaptureBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcScreenCaptureBridge);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_SCREEN_CAPTURE_ARC_SCREEN_CAPTURE_BRIDGE_H_
