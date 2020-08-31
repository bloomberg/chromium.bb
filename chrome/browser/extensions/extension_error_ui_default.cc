// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_error_ui_default.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/browser/blacklist_state.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

base::string16 GenerateTitle(const ExtensionSet& forbidden) {
  int app_count = 0;
  int extension_count = 0;
  for (const auto& extension : forbidden) {
    if (extension->is_app())
      app_count++;
    else
      extension_count++;
  }

  if ((app_count > 0) && (extension_count > 0)) {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_AND_APP_ALERT_TITLE);
  }
  if (app_count > 0) {
    return l10n_util::GetPluralStringFUTF16(IDS_APP_ALERT_TITLE, app_count);
  }
  return l10n_util::GetPluralStringFUTF16(IDS_EXTENSION_ALERT_TITLE,
                                          extension_count);
}

std::vector<base::string16> GenerateMessage(
    const ExtensionSet& forbidden,
    content::BrowserContext* browser_context) {
  std::vector<base::string16> message;
  message.reserve(forbidden.size());
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context);
  for (const auto& extension : forbidden) {
    BlacklistState blacklist_state =
        prefs->GetExtensionBlacklistState(extension->id());
    bool disable_remotely_for_malware = prefs->HasDisableReason(
        extension->id(), disable_reason::DISABLE_REMOTELY_FOR_MALWARE);
    int id = 0;
    if (disable_remotely_for_malware ||
        (blacklist_state == BlacklistState::BLACKLISTED_MALWARE)) {
      id = forbidden.size() == 1
               ? IDS_EXTENSION_ALERT_ITEM_BLACKLISTED_MALWARE
               : IDS_EXTENSION_ALERT_ITEM_BLACKLISTED_MALWARE_PLURAL;
    } else {
      id = extension->is_app() ? IDS_APP_ALERT_ITEM_BLACKLISTED_OTHER
                               : IDS_EXTENSION_ALERT_ITEM_BLACKLISTED_OTHER;
    }
    message.push_back(
        l10n_util::GetStringFUTF16(id, base::UTF8ToUTF16(extension->name())));
  }
  return message;
}

}  // namespace

class ExtensionGlobalError : public GlobalErrorWithStandardBubble {
 public:
  explicit ExtensionGlobalError(ExtensionErrorUI::Delegate* delegate)
      : delegate_(delegate) {}

 private:
  // GlobalError overrides:
  bool HasMenuItem() override { return false; }

  int MenuItemCommandID() override {
    NOTREACHED();
    return 0;
  }

  base::string16 MenuItemLabel() override {
    NOTREACHED();
    return {};
  }

  void ExecuteMenuItem(Browser* browser) override { NOTREACHED(); }

  base::string16 GetBubbleViewTitle() override {
    return GenerateTitle(delegate_->GetBlacklistedExtensions());
  }

  std::vector<base::string16> GetBubbleViewMessages() override {
    return GenerateMessage(delegate_->GetBlacklistedExtensions(),
                           delegate_->GetContext());
  }

  base::string16 GetBubbleViewAcceptButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_OK);
  }

  base::string16 GetBubbleViewCancelButtonLabel() override { return {}; }

  base::string16 GetBubbleViewDetailsButtonLabel() override {
    return l10n_util::GetStringUTF16(IDS_EXTENSION_ALERT_ITEM_DETAILS);
  }

  void OnBubbleViewDidClose(Browser* browser) override {
    delegate_->OnAlertClosed();
  }

  void BubbleViewAcceptButtonPressed(Browser* browser) override {
    delegate_->OnAlertAccept();
  }

  void BubbleViewCancelButtonPressed(Browser* browser) override {
    NOTREACHED();
  }

  void BubbleViewDetailsButtonPressed(Browser* browser) override {
    delegate_->OnAlertDetails();
  }

  ExtensionErrorUI::Delegate* delegate_;

  ExtensionGlobalError(const ExtensionGlobalError&) = delete;
  ExtensionGlobalError& operator=(const ExtensionGlobalError&) = delete;
};

ExtensionErrorUIDefault::ExtensionErrorUIDefault(
    ExtensionErrorUI::Delegate* delegate)
    : profile_(Profile::FromBrowserContext(delegate->GetContext())),
      global_error_(std::make_unique<ExtensionGlobalError>(delegate)) {}

ExtensionErrorUIDefault::~ExtensionErrorUIDefault() = default;

bool ExtensionErrorUIDefault::ShowErrorInBubbleView() {
  Browser* browser = chrome::FindLastActiveWithProfile(profile_);
  if (!browser)
    return false;

  browser_ = browser;
  global_error_->ShowBubbleView(browser);
  return true;
}

void ExtensionErrorUIDefault::ShowExtensions() {
  DCHECK(browser_);
  chrome::ShowExtensions(browser_, std::string());
}

void ExtensionErrorUIDefault::Close() {
  if (global_error_->HasShownBubbleView()) {
    // Will end up calling into |global_error_|->OnBubbleViewDidClose,
    // possibly synchronously.
    global_error_->GetBubbleView()->CloseBubbleView();
  }
}

GlobalErrorWithStandardBubble* ExtensionErrorUIDefault::GetErrorForTesting() {
  return global_error_.get();
}

}  // namespace extensions
