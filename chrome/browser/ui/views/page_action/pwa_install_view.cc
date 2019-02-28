// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"
#include "components/omnibox/browser/vector_icons.h"

PwaInstallView::PwaInstallView(CommandUpdater* command_updater,
                               PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater, IDC_INSTALL_PWA, delegate) {
  SetVisible(false);
}

PwaInstallView::~PwaInstallView() {}

bool PwaInstallView::Update() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return false;

  bool is_installable =
      banners::AppBannerManager::IsWebContentsInstallable(web_contents);
  bool is_installed =
      web_app::WebAppTabHelperBase::FromWebContents(web_contents)
          ->HasAssociatedApp();
  bool show_install_button = is_installable && !is_installed;
  // TODO(crbug.com/907351): When installability is unknown and we're still in
  // the scope of a previously-determined installable site, display it as still
  // being installable.

  bool was_visible = visible();
  SetVisible(show_install_button);
  return visible() != was_visible;
}

void PwaInstallView::OnExecuting(PageActionIconView::ExecuteSource source) {}

views::BubbleDialogDelegateView* PwaInstallView::GetBubble() const {
  // TODO(https://907351): Implement.
  return nullptr;
}

const gfx::VectorIcon& PwaInstallView::GetVectorIcon() const {
  return omnibox::kPlusIcon;
}

base::string16 PwaInstallView::GetTextForTooltipAndAccessibleName() const {
  // TODO(https://907351): Implement.
  return base::string16();
}
