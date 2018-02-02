// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_MANAGER_ANDROID_H_
#define CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_MANAGER_ANDROID_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

struct InstallableData;
class InstallableManager;

// Manages an infobar shown to users when they visit a progressive web app.
// Tapping the infobar triggers the add to home screen flow.
class PwaAmbientBadgeManagerAndroid : public content::WebContentsObserver {
 public:
  explicit PwaAmbientBadgeManagerAndroid(content::WebContents* contents);
  ~PwaAmbientBadgeManagerAndroid() override;
  void MaybeShowBadge();

 private:
  void OnDidFinishInstallableCheck(const InstallableData& data);

  InstallableManager* installable_manager_;
  bool can_install_webapk_;
  base::WeakPtrFactory<PwaAmbientBadgeManagerAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PwaAmbientBadgeManagerAndroid);
};

#endif  // CHROME_BROWSER_INSTALLABLE_PWA_AMBIENT_BADGE_MANAGER_ANDROID_H_
