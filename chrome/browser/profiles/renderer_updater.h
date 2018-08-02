// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
#define CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_

#include "base/macros.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager.h"

class Profile;

namespace content {
class RenderProcessHost;
}

// The RendererUpdater is responsible for updating renderers about state change.
class RendererUpdater : public KeyedService,
                        public SigninManagerBase::Observer {
 public:
  explicit RendererUpdater(Profile* profile);
  ~RendererUpdater() override;

  // KeyedService:
  void Shutdown() override;

  // Initialize a newly-started renderer process.
  void InitializeRenderer(content::RenderProcessHost* render_process_host);

 private:
  // Update the signin state for all renderers due to a signin notification.
  void UpdateRenderersSignin();

  std::vector<chrome::mojom::RendererConfigurationAssociatedPtr>
  GetRendererConfigurations();

  chrome::mojom::RendererConfigurationAssociatedPtr GetRendererConfiguration(
      content::RenderProcessHost* render_process_host);

  // SigninManagerBase::Observer:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

  Profile* profile_;
  SigninManagerBase* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(RendererUpdater);
};

#endif  // CHROME_BROWSER_PROFILES_RENDERER_UPDATER_H_
