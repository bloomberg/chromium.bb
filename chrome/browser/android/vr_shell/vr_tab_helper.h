// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_VR_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_VR_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_user_data.h"

namespace vr_shell {

class VrTabHelper : public content::WebContentsUserData<VrTabHelper> {
 public:
  ~VrTabHelper() override;

  bool is_in_vr() const { return is_in_vr_; }

  // Called by VrShell when we enter and exit vr mode. It finds us by looking us
  // up on the WebContents. Eventually, we will also set a number of flags here
  // as we enter and exit vr mode (see TODO below).
  void SetIsInVr(bool is_in_vr);

  static bool IsInVr(content::WebContents* contents);

 private:
  friend class content::WebContentsUserData<VrTabHelper>;

  explicit VrTabHelper(content::WebContents* contents);

  // TODO(asimjour): once we have per-dialog flags for disabling specific
  // content-related popups, we should hang onto a pointer to the web contents
  // and set those flags as we enter and exit vr mode.
  bool is_in_vr_ = false;

  DISALLOW_COPY_AND_ASSIGN(VrTabHelper);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_VR_TAB_HELPER_H_
