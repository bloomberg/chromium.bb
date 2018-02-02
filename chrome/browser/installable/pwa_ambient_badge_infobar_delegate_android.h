// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "components/infobars/core/infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

struct ShortcutInfo;

namespace content {
class WebContents;
}

// Delegate for a infobar shown to users when they visit a progressive web app.
// Tapping the infobar triggers the add to home screen flow.
class PwaAmbientBadgeInfoBarDelegateAndroid : public infobars::InfoBarDelegate {
 public:
  ~PwaAmbientBadgeInfoBarDelegateAndroid() override;

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents,
                     std::unique_ptr<ShortcutInfo> info,
                     const SkBitmap& primary_icon,
                     const SkBitmap& badge_icon,
                     bool is_webapk);

  void AddToHomescreen();
  const SkBitmap& GetPrimaryIcon() const;

 private:
  PwaAmbientBadgeInfoBarDelegateAndroid(std::unique_ptr<ShortcutInfo> info,
                                        const SkBitmap& primary_icon,
                                        const SkBitmap& badge_icon,
                                        bool is_webapk);

  // InfoBarDelegate overrides:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  std::unique_ptr<ShortcutInfo> shortcut_info_;
  const SkBitmap primary_icon_;
  const SkBitmap badge_icon_;
  bool is_webapk_;

  DISALLOW_COPY_AND_ASSIGN(PwaAmbientBadgeInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_INFOBAR_DELEGATE_ANDROID_H_
