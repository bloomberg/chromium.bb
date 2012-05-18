// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_BUNDLE_INSTALLED_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_BUNDLE_INSTALLED_BUBBLE_GTK_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"

class Browser;

// The GTK implementation of the bundle installed bubble. The bubble reports
// which extensions and apps installed or failed when the bundle install
// completes.
class BundleInstalledBubbleGtk
    : public BubbleDelegateGtk,
      public base::RefCounted<BundleInstalledBubbleGtk> {
 public:
  // Displays an installed bubble in the |browser| for the |bundle|.
  BundleInstalledBubbleGtk(const extensions::BundleInstaller* bundle,
                           Browser* browser);

 private:
  friend class base::RefCounted<BundleInstalledBubbleGtk>;

  virtual ~BundleInstalledBubbleGtk();

  // Assembles the content area of the bubble.
  void ShowInternal(const extensions::BundleInstaller* bundle);

  // The bubble lists the items that installed successfully and those that
  // failed. This assembles the lists for items in the given |state|.
  void InsertExtensionList(GtkWidget* parent,
                           const extensions::BundleInstaller* bundle,
                           extensions::BundleInstaller::Item::State state);

  // BubbleDelegateGtk, called when the bubble is about to close.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // Closes the bubble.
  void Close();

  // Called when the user clicks the bubble's close button.
  static void OnButtonClick(GtkWidget* button,
                            BundleInstalledBubbleGtk* bubble);

  Browser* browser_;
  scoped_ptr<CustomDrawButton> close_button_;
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(BundleInstalledBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_BUNDLE_INSTALLED_BUBBLE_GTK_H_
