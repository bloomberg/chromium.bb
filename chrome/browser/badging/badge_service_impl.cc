// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_impl.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

// static
void BadgeServiceImpl::Create(blink::mojom::BadgeServiceRequest request,
                              content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);

  auto* browser_context =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetBrowserContext();
  auto* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(browser_context));

  // Lifetime managed through FrameServiceBase.
  new BadgeServiceImpl(render_frame_host, browser_context, badge_manager,
                       std::move(request));
}

void BadgeServiceImpl::SetBadge() {
  const extensions::Extension* extension = ExtensionFromLastUrl();

  if (!extension)
    return;

  badge_manager_->UpdateBadge(extension->id(), base::nullopt);
}

void BadgeServiceImpl::ClearBadge() {
  const extensions::Extension* extension = ExtensionFromLastUrl();

  if (!extension)
    return;

  badge_manager_->ClearBadge(extension->id());
}

BadgeServiceImpl::BadgeServiceImpl(content::RenderFrameHost* render_frame_host,
                                   content::BrowserContext* browser_context,
                                   badging::BadgeManager* badge_manager,
                                   blink::mojom::BadgeServiceRequest request)
    : content::FrameServiceBase<blink::mojom::BadgeService>(render_frame_host,
                                                            std::move(request)),
      render_frame_host_(render_frame_host),
      browser_context_(browser_context),
      badge_manager_(badge_manager) {}

BadgeServiceImpl::~BadgeServiceImpl() = default;

const extensions::Extension* BadgeServiceImpl::ExtensionFromLastUrl() {
  DCHECK(browser_context_);

  return extensions::util::GetInstalledPwaForUrl(
      browser_context_, render_frame_host_->GetLastCommittedURL());
}
