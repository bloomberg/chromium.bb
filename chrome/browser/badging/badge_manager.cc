// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_MACOSX)
#include "chrome/browser/badging/badge_manager_delegate_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/badging/badge_manager_delegate_win.h"
#endif

namespace badging {

std::string GetBadgeString(base::Optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > kMaxBadgeContent) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(kMaxBadgeContent)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}

BadgeManager::BadgeManager(Profile* profile) {
#if defined(OS_MACOSX)
  SetDelegate(std::make_unique<BadgeManagerDelegateMac>(profile));
#elif defined(OS_WIN)
  SetDelegate(std::make_unique<BadgeManagerDelegateWin>(profile));
#endif
}

BadgeManager::~BadgeManager() = default;

void BadgeManager::UpdateBadge(const std::string& app_id,
                               base::Optional<uint64_t> content) {
  badged_apps_[app_id] = content;

  if (!delegate_)
    return;

  delegate_->OnBadgeSet(app_id, content);
}

void BadgeManager::ClearBadge(const std::string& app_id) {
  badged_apps_.erase(app_id);
  if (!delegate_)
    return;

  delegate_->OnBadgeCleared(app_id);
}

void BadgeManager::OnBadgeChangeIgnored() {
  if (!delegate_)
    return;

  delegate_->OnBadgeChangeIgnoredForTesting();
}

void BadgeManager::SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

}  // namespace badging
