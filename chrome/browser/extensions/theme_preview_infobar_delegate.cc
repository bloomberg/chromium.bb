// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/theme_preview_infobar_delegate.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"

ThemePreviewInfobarDelegate::ThemePreviewInfobarDelegate(
    TabContents* tab_contents, const std::string& name)
         : ConfirmInfoBarDelegate(tab_contents),
           profile_(tab_contents->profile()), name_(name) {
}

bool ThemePreviewInfobarDelegate::EqualsDelegate(InfoBarDelegate* delegate)
    const {
  // If another infobar of this type is showing, this will prevent us adding
  // a new one, we only care if they're the same type, as pressing undo always
  // has the same result each time. This does mean that the text continues
  // to refer to the old theme, but this is good enough for beta.
  // http://crbug.com/17932
  if (delegate->AsThemePreviewInfobarDelegate())
    return true;

  return false;
}

void ThemePreviewInfobarDelegate::InfoBarClosed() {
  delete this;
}

std::wstring ThemePreviewInfobarDelegate::GetMessageText() const {
  return l10n_util::GetStringF(IDS_THEME_PREVIEW_INFOBAR_LABEL,
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
      return l10n_util::GetString(IDS_THEME_PREVIEW_INFOBAR_UNDO_BUTTON);
    default:
      return L"";
  }
}

bool ThemePreviewInfobarDelegate::Cancel() {
  // Blech, this is a total hack.
  //
  // a) We should be uninstalling via ExtensionsService, not
  //    Profile::ClearTheme().
  // b) We should be able to view the theme without installing it. This would
  //    help in edge cases like the user closing the window or tab before making
  //    a decision.
  profile_->ClearTheme();
  return true;
}
