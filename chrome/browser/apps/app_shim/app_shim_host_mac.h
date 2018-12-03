// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
#define CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"
#include "chrome/common/mac/app_shim.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class NSViewBridgeFactoryHost;
}  // namespace content

namespace views {
class BridgeFactoryHost;
}  // namespace views

class AppShimHostBootstrap;

// This is the counterpart to AppShimController in
// chrome/app/chrome_main_app_mode_mac.mm. The AppShimHost owns itself, and is
// destroyed when the app it corresponds to is closed or when the channel
// connected to the app shim is closed.
class AppShimHost : public chrome::mojom::AppShimHost,
                    public apps::AppShimHandler::Host {
 public:
  AppShimHost(const std::string& app_id, const base::FilePath& profile_path);

  // apps::AppShimHandler::Host overrides:
  bool HasBootstrapConnected() const override;
  void OnBootstrapConnected(
      std::unique_ptr<AppShimHostBootstrap> bootstrap) override;
  void OnAppLaunchComplete(apps::AppShimLaunchResult result) override;
  void OnAppClosed() override;
  void OnAppHide() override;
  void OnAppUnhideWithoutActivation() override;
  void OnAppRequestUserAttention(apps::AppShimAttentionType type) override;
  base::FilePath GetProfilePath() const override;
  std::string GetAppId() const override;
  views::BridgeFactoryHost* GetViewsBridgeFactoryHost() const override;

 protected:
  // AppShimHost is owned by itself. It will delete itself in Close (called on
  // channel error and OnAppClosed).
  ~AppShimHost() override;
  void ChannelError(uint32_t custom_reason, const std::string& description);
  void SendLaunchResult();

  // Closes the channel and destroys the AppShimHost.
  void Close();

  // Return the AppShimHandler for this app (virtual for tests).
  virtual apps::AppShimHandler* GetAppShimHandler() const;

  // chrome::mojom::AppShimHost.
  void FocusApp(apps::AppShimFocusType focus_type,
                const std::vector<base::FilePath>& files) override;
  void SetAppHidden(bool hidden) override;
  void QuitApp() override;

  mojo::Binding<chrome::mojom::AppShimHost> host_binding_;
  chrome::mojom::AppShimPtr app_shim_;
  chrome::mojom::AppShimRequest app_shim_request_;

  std::unique_ptr<AppShimHostBootstrap> bootstrap_;

  std::unique_ptr<views::BridgeFactoryHost> views_bridge_factory_host_;
  std::unique_ptr<content::NSViewBridgeFactoryHost> content_bridge_factory_;

  std::string app_id_;
  base::FilePath profile_path_;

  // The result passed to OnAppLaunchComplete, not set until OnAppLaunchComplete
  // is called.
  base::Optional<apps::AppShimLaunchResult> launch_result_;
  bool has_sent_on_launch_complete_ = false;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(AppShimHost);
};

#endif  // CHROME_BROWSER_APPS_APP_SHIM_APP_SHIM_HOST_MAC_H_
