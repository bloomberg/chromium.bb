// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/blocked_popup_handler.h"

#include <memory>
#include <utility>

#include "base/format_macros.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/web/public/referrer.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace {
// The infobar to display when a popup is blocked.
class BlockPopupInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  BlockPopupInfoBarDelegate(ios::ChromeBrowserState* browser_state,
                            const std::vector<web::BlockedPopupInfo>& popups)
      : browser_state_(browser_state), popups_(popups) {}

  ~BlockPopupInfoBarDelegate() override {}

  InfoBarIdentifier GetIdentifier() const override {
    return POPUP_BLOCKED_INFOBAR_DELEGATE;
  }

  gfx::Image GetIcon() const override {
    if (icon_.IsEmpty()) {
      icon_ = gfx::Image([UIImage imageNamed:@"infobar_popup_blocker"]);
    }
    return icon_;
  }

  infobars::InfoBarDelegate::Type GetInfoBarType() const override {
    return WARNING_TYPE;
  }

  base::string16 GetMessageText() const override {
    return l10n_util::GetStringFUTF16(
        IDS_IOS_POPUPS_BLOCKED_MOBILE,
        base::UTF8ToUTF16(base::StringPrintf("%" PRIuS, popups_.size())));
  }

  base::string16 GetButtonLabel(InfoBarButton button) const override {
    DCHECK(button == BUTTON_OK);
    return l10n_util::GetStringUTF16(IDS_IOS_POPUPS_ALWAYS_SHOW_MOBILE);
  }

  bool Accept() override {
    std::vector<web::BlockedPopupInfo>::iterator it;
    scoped_refptr<HostContentSettingsMap> host_content_map_settings(
        ios::HostContentSettingsMapFactory::GetForBrowserState(browser_state_));
    for (it = popups_.begin(); it != popups_.end(); ++it) {
      it->ShowPopup();
      host_content_map_settings->SetContentSettingCustomScope(
          ContentSettingsPattern::FromURL(it->referrer().url),
          ContentSettingsPattern::Wildcard(), CONTENT_SETTINGS_TYPE_POPUPS,
          std::string(), CONTENT_SETTING_ALLOW);
    }
    return true;
  }

  int GetButtons() const override { return BUTTON_OK; }

 private:
  // The browser state to access user preferences.
  ios::ChromeBrowserState* browser_state_;
  // The popups to open.
  std::vector<web::BlockedPopupInfo> popups_;
  // The icon to display.
  mutable gfx::Image icon_;
};
}  // namespace

BlockedPopupHandler::BlockedPopupHandler(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state),
      delegate_(nullptr),
      infobar_(nullptr),
      scoped_observer_(this) {}

BlockedPopupHandler::~BlockedPopupHandler() {}

void BlockedPopupHandler::SetDelegate(
    id<BlockedPopupHandlerDelegate> delegate) {
  scoped_observer_.RemoveAll();
  delegate_ = delegate;
  if (delegate_)
    scoped_observer_.Add([delegate_ infoBarManager]);
}

void BlockedPopupHandler::HandlePopup(
    const web::BlockedPopupInfo& blocked_popup_info) {
  if (!delegate_)
    return;

  popups_.push_back(blocked_popup_info);
  ShowInfoBar();
}

void BlockedPopupHandler::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                           bool animate) {
  if (infobar == infobar_) {
    infobar_ = nullptr;
    popups_.clear();
  }
}

void BlockedPopupHandler::OnManagerShuttingDown(
    infobars::InfoBarManager* infobar_manager) {
  scoped_observer_.Remove(infobar_manager);
}

void BlockedPopupHandler::ShowInfoBar() {
  if (!popups_.size() || !delegate_)
    return;
  infobars::InfoBarManager* infobar_manager = [delegate_ infoBarManager];
  std::unique_ptr<infobars::InfoBar> infobar =
      infobar_manager->CreateConfirmInfoBar(
          std::unique_ptr<ConfirmInfoBarDelegate>(
              new BlockPopupInfoBarDelegate(browser_state_, popups_)));
  if (infobar_) {
    infobar_ = infobar_manager->ReplaceInfoBar(infobar_, std::move(infobar));
  } else {
    infobar_ = infobar_manager->AddInfoBar(std::move(infobar));
  }
}
