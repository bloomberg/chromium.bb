// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/vr_tab_helper.h"

#include "chrome/browser/android/vr_shell/vr_shell_delegate.h"
#include "device/vr/android/gvr/gvr_delegate_provider.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(vr_shell::VrTabHelper);

namespace vr_shell {

VrTabHelper::VrTabHelper(content::WebContents* contents) {}
VrTabHelper::~VrTabHelper() {}

void VrTabHelper::SetIsInVr(bool is_in_vr) {
  is_in_vr_ = is_in_vr;
}

/* static */
bool VrTabHelper::IsInVr(content::WebContents* contents) {
  VrTabHelper* vr_tab_helper = VrTabHelper::FromWebContents(contents);
  return vr_tab_helper->is_in_vr();
}

}  // namespace vr_shell
