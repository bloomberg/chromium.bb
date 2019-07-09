// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

// static
void BadgeServiceImpl::Create(blink::mojom::BadgeServiceRequest request,
                              content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  // Lifetime managed through FrameServiceBase.
  new BadgeServiceImpl(render_frame_host, std::move(request));
}

void BadgeServiceImpl::SetInteger(uint64_t content) {
  SetBadge(base::Optional<uint64_t>(content));
}

void BadgeServiceImpl::SetFlag() {
  SetBadge(base::nullopt);
}

void BadgeServiceImpl::SetBadge(base::Optional<uint64_t> content) {
  auto app_id = GetAppIdToBadge();
  if (!app_id) {
    badge_manager_->OnBadgeChangeIgnored();
    return;
  }

  badge_manager_->UpdateBadge(app_id.value(), content);
}

void BadgeServiceImpl::ClearBadge() {
  auto app_id = GetAppIdToBadge();
  if (!app_id) {
    badge_manager_->OnBadgeChangeIgnored();
    return;
  }

  badge_manager_->ClearBadge(app_id.value());
}

BadgeServiceImpl::BadgeServiceImpl(content::RenderFrameHost* render_frame_host,
                                   blink::mojom::BadgeServiceRequest request)
    : content::FrameServiceBase<blink::mojom::BadgeService>(render_frame_host,
                                                            std::move(request)),
      render_frame_host_(render_frame_host) {
  web_contents_ = content::WebContents::FromRenderFrameHost(render_frame_host_);
  browser_context_ = web_contents_->GetBrowserContext();
  badge_manager_ = badging::BadgeManagerFactory::GetInstance()->GetForProfile(
      Profile::FromBrowserContext(browser_context_));
}

BadgeServiceImpl::~BadgeServiceImpl() = default;

base::Optional<std::string> BadgeServiceImpl::GetAppIdToBadge() const {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  if (!browser)
    return base::nullopt;

  auto* app_controller = browser->app_controller();
  if (!app_controller)
    return base::nullopt;

  bool in_app = app_controller->IsUrlInAppScope(
      render_frame_host_->GetLastCommittedURL());

  // If the current frame is not in the app, don't apply a badge.
  return in_app ? app_controller->GetAppId() : base::nullopt;
}
