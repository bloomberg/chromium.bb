// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_updater.h"

#include "chrome/common/renderer_configuration.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

SigninUpdater::SigninUpdater(SigninManagerBase* signin_manager)
    : signin_manager_(signin_manager) {
  DCHECK(signin_manager_);
  signin_manager_->AddObserver(this);
}

SigninUpdater::~SigninUpdater() {
  DCHECK(!signin_manager_);
}

void SigninUpdater::Shutdown() {
  signin_manager_->RemoveObserver(this);
  signin_manager_ = nullptr;
}

void SigninUpdater::GoogleSigninSucceeded(const AccountInfo& account_info) {
  UpdateRenderer(true /* is_signed_in */);
}

void SigninUpdater::GoogleSignedOut(const AccountInfo& account_info) {
  UpdateRenderer(false /* is_signed_in */);
}

void SigninUpdater::UpdateRenderer(bool is_signed_in) {
  for (content::RenderProcessHost::iterator it(
           content::RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    IPC::ChannelProxy* channel = it.GetCurrentValue()->GetChannel();
    if (channel) {
      chrome::mojom::RendererConfigurationAssociatedPtr renderer_configuration;
      channel->GetRemoteAssociatedInterface(&renderer_configuration);
      renderer_configuration->SetIsSignedIn(is_signed_in);
    }
  }
}
