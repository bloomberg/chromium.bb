// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_EXTENSION_APP_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_EXTENSION_APP_ITEM_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/views/aura/app_list/chrome_app_list_item.h"
#include "chrome/common/string_ordinal.h"

class Extension;
class ExtensionResource;
class Profile;
class SkBitmap;

// ExtensionAppItem represents an extension app in app list.
class ExtensionAppItem : public ChromeAppListItem,
                         public ImageLoadingTracker::Observer {
 public:
  ExtensionAppItem(Profile* profile, const Extension* extension);
  virtual ~ExtensionAppItem();

  // Gets extension associated with this model. Returns NULL if extension
  // no longer exists.
  const Extension* GetExtension() const;

  int page_index() const {
    return page_index_;
  }

  const StringOrdinal& launch_ordinal() const {
    return launch_ordinal_;
  }

 private:
  // Loads extension icon.
  void LoadImage(const Extension* extension);

  // Loads default extension icon.
  void LoadDefaultImage();

  // Overridden from ImageLoadingTracker::Observer
  virtual void OnImageLoaded(SkBitmap* image,
                             const ExtensionResource& resource,
                             int tracker_index) OVERRIDE;

  // Overridden from ChromeAppListItem:
  virtual void Activate(int event_flags) OVERRIDE;

  Profile* profile_;
  const std::string extension_id_;

  const int page_index_;
  const StringOrdinal launch_ordinal_;

  scoped_ptr<ImageLoadingTracker> tracker_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppItem);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AURA_APP_LIST_EXTENSION_APP_ITEM_H_
