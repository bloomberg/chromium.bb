// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_tab_helper.h"

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/web_preferences.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"

using content::WebContents;
using content::WebPreferences;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(vr_shell::VrTabHelper);

namespace vr_shell {

VrTabHelper::VrTabHelper(content::WebContents* contents)
    : web_contents_(contents) {}

VrTabHelper::~VrTabHelper() {}

void VrTabHelper::SetIsInVr(bool is_in_vr) {
  if (is_in_vr_ == is_in_vr)
    return;

  is_in_vr_ = is_in_vr;

  WebPreferences web_prefs =
      web_contents_->GetRenderViewHost()->GetWebkitPreferences();
  web_prefs.page_popups_suppressed = is_in_vr_;
  web_contents_->GetRenderViewHost()->UpdateWebkitPreferences(web_prefs);
}

/* static */
bool VrTabHelper::IsInVr(content::WebContents* contents) {
  VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
  if (!vr_tab_helper) {
    // This can only happen for unittests.
    VrTabHelper::CreateForWebContents(contents);
    vr_tab_helper = VrTabHelper::FromWebContents(contents);
  }
  return vr_tab_helper->is_in_vr();
}

}  // namespace vr_shell
