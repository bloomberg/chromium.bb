// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
#pragma once

#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "ui/gfx/gtk_util.h"

class ExtensionInfobarDelegate;
class ExtensionResource;
class ExtensionViewGtk;

class ExtensionInfoBarGtk : public InfoBarGtk,
                            public ImageLoadingTracker::Observer {
 public:
  ExtensionInfoBarGtk(TabContentsWrapper* owner,
                      ExtensionInfoBarDelegate* delegate);
  virtual ~ExtensionInfoBarGtk();

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(
      SkBitmap* image, const ExtensionResource& resource, int index);

 private:
  // Build the widgets of the Infobar.
  void BuildWidgets();

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, void, OnSizeAllocate,
                       GtkAllocation*);

  ImageLoadingTracker tracker_;

  ExtensionInfoBarDelegate* delegate_;

  ExtensionViewGtk* view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
