// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

// Delegate for a infobar shown to users when they visit a progressive web app.
// Tapping the infobar triggers the add to home screen flow.
class InstallableAmbientBadgeInfoBarDelegate
    : public infobars::InfoBarDelegate {
 public:
  class Client {
   public:
    // Called to trigger the add to home screen flow.
    virtual void AddToHomescreenFromBadge() = 0;

    virtual ~Client() {}
  };

  ~InstallableAmbientBadgeInfoBarDelegate() override;

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents,
                     base::WeakPtr<Client> weak_client,
                     const SkBitmap& primary_icon,
                     const GURL& start_url,
                     bool is_installed);

  void AddToHomescreen();
  const SkBitmap& GetPrimaryIcon() const;
  const GURL& GetUrl() const { return start_url_; }
  bool is_installed() const { return is_installed_; }

 private:
  InstallableAmbientBadgeInfoBarDelegate(base::WeakPtr<Client> weak_client,
                                         const SkBitmap& primary_icon,
                                         const GURL& start_url,
                                         bool is_installed);

  // InfoBarDelegate overrides:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;

  base::WeakPtr<Client> weak_client_;
  const SkBitmap primary_icon_;
  const GURL& start_url_;

  // Whether the current site is already installed.
  bool is_installed_;

  DISALLOW_COPY_AND_ASSIGN(InstallableAmbientBadgeInfoBarDelegate);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
