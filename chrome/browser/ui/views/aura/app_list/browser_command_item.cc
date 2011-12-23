// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list/browser_command_item.h"

#include "chrome/browser/ui/browser.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

BrowserCommandItem::BrowserCommandItem(Browser* browser,
                                       int command_id,
                                       int title_id,
                                       int icon_id)
    : browser_(browser),
      command_id_(command_id) {
  SetTitle(l10n_util::GetStringUTF8(title_id));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SetIcon(*rb.GetImageNamed(icon_id).ToSkBitmap());
}

BrowserCommandItem::~BrowserCommandItem() {
}

void BrowserCommandItem::Activate(int event_flags) {
  browser_->ExecuteCommand(command_id_, event_flags);
}
