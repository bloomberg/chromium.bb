// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_MANAGED_PREFS_BANNER_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_MANAGED_PREFS_BANNER_VIEW_H_
#pragma once

#include "chrome/browser/policy/managed_prefs_banner_base.h"
#include "views/view.h"

namespace views {
class ImageView;
class Label;
}

// Displays a banner showing a warning message that tells the user some options
// cannot be changed because the relevant preferences are managed by their
// system administrator.
class ManagedPrefsBannerView : public policy::ManagedPrefsBannerBase,
                               public views::View {
 public:
  // Initialize the banner. |page| is used to determine the names of the
  // preferences that control the banner visibility through their managed flag.
  ManagedPrefsBannerView(PrefService* pref_service, OptionsPage page);
  virtual ~ManagedPrefsBannerView() {}

 private:
  // Initialize contents and layout.
  void Init();

  // views::View overrides.
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // ManagedPrefsBannerBase override.
  virtual void OnUpdateVisibility();

  // Holds the warning icon image and text label and renders the border.
  views::View* content_;
  // Warning icon image.
  views::ImageView* warning_image_;
  // The label responsible for rendering the warning text.
  views::Label* label_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ManagedPrefsBannerView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_MANAGED_PREFS_BANNER_VIEW_H_
