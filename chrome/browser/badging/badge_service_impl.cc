// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/badging/badge_manager_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

BadgeServiceImpl::BadgeServiceImpl(content::RenderFrameHost* render_frame_host,
                                   content::BrowserContext* browser_context,
                                   badging::BadgeManager* badge_manager)
    : render_frame_host_(render_frame_host),
      browser_context_(browser_context),
      badge_manager_(badge_manager) {}
BadgeServiceImpl::~BadgeServiceImpl() = default;

// static
void BadgeServiceImpl::Create(blink::mojom::BadgeServiceRequest request,
                              content::RenderFrameHost* render_frame_host) {
  auto* browser_context =
      content::WebContents::FromRenderFrameHost(render_frame_host)
          ->GetBrowserContext();
  auto* badge_manager =
      badging::BadgeManagerFactory::GetInstance()->GetForProfile(
          Profile::FromBrowserContext(browser_context));
  mojo::MakeStrongBinding(
      std::make_unique<BadgeServiceImpl>(render_frame_host, browser_context,
                                         badge_manager),
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

// private
const extensions::Extension* BadgeServiceImpl::ExtensionFromLastUrl() {
  DCHECK(render_frame_host_);
  DCHECK(browser_context_);

  return extensions::util::GetInstalledPwaForUrl(
      browser_context_, render_frame_host_->GetLastCommittedURL());
}
