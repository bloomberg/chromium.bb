// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/credentials_item_view.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {
const int kButtonHeight = 50;
}

CredentialsItemView::CredentialsItemView(views::ButtonListener* button_listener,
                                         const base::string16& text)
    : LabelButton(button_listener, text) {
  SetMinSize(gfx::Size(0, kButtonHeight));
  // TODO(vasilii): temporary code below shows the built-in profile icon instead
  // of avatar.
  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  const gfx::Image& image = cache.GetAvatarIconOfProfileAtIndex(0);
  SetImage(STATE_NORMAL, *profiles::GetSizedAvatarIcon(
      image, true, kButtonHeight, kButtonHeight).ToImageSkia());
  SetFocusable(true);
}

CredentialsItemView::~CredentialsItemView() {
}
