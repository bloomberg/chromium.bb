// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_preview_infobar_delegate.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"

ThemePreviewInfobarDelegate::ThemePreviewInfobarDelegate(
    TabContents* tab_contents, const std::string& name,
    const std::string& previous_theme_id)
         : ConfirmInfoBarDelegate(tab_contents),
           profile_(tab_contents->profile()), name_(name),
           previous_theme_id_(previous_theme_id) {
}

void ThemePreviewInfobarDelegate::InfoBarClosed() {
  delete this;
}

std::wstring ThemePreviewInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringF(IDS_THEME_INSTALL_INFOBAR_LABEL,
                               UTF8ToWide(name_));
}

SkBitmap* ThemePreviewInfobarDelegate::GetIcon() const {
  // TODO(aa): Reply with the theme's icon, but this requires reading it
  // asynchronously from disk.
  return NULL;
}

ThemePreviewInfobarDelegate*
    ThemePreviewInfobarDelegate::AsThemePreviewInfobarDelegate() {
  return this;
}

int ThemePreviewInfobarDelegate::GetButtons() const {
  return BUTTON_CANCEL;
}

std::wstring ThemePreviewInfobarDelegate::GetButtonLabel(
    ConfirmInfoBarDelegate::InfoBarButton button) const {
  switch (button) {
    case BUTTON_CANCEL:
      // TODO(aa): Reusing IDS_UNDO is hack to get around string freeze. This
      // should be changed back to IDS_THEME_INSTALL_INFOBAR_UNDO_BUTTON at some
      // point.
      return l10n_util::GetString(IDS_UNDO);
    default:
      return L"";
  }
}

bool ThemePreviewInfobarDelegate::Cancel() {
  if (!previous_theme_id_.empty()) {
    ExtensionsService* service = profile_->GetExtensionsService();
    if (service) {
      Extension* previous_theme = service->GetExtensionById(previous_theme_id_);
      if (previous_theme) {
        profile_->SetTheme(previous_theme);
        return true;
      }
    }
  }

  profile_->ClearTheme();
  return true;
}
