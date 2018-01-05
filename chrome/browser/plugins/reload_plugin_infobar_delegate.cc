// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/reload_plugin_infobar_delegate.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ReloadPluginInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    content::NavigationController* controller,
    const base::string16& message) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new ReloadPluginInfoBarDelegate(controller, message))));
}

ReloadPluginInfoBarDelegate::ReloadPluginInfoBarDelegate(
    content::NavigationController* controller,
    const base::string16& message)
    : controller_(controller), message_(message) {}

ReloadPluginInfoBarDelegate::~ReloadPluginInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReloadPluginInfoBarDelegate::GetIdentifier() const {
  return RELOAD_PLUGIN_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& ReloadPluginInfoBarDelegate::GetVectorIcon() const {
  return kExtensionCrashedIcon;
}

base::string16 ReloadPluginInfoBarDelegate::GetMessageText() const {
  return message_;
}

int ReloadPluginInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 ReloadPluginInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_RELOAD_PAGE_WITH_PLUGIN);
}

bool ReloadPluginInfoBarDelegate::Accept() {
  controller_->Reload(content::ReloadType::NORMAL, true);
  return true;
}
