// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/permissions/permission_infobar_delegate.h"

// MidiPermissionInfoBarDelegateAndroids are created by the
// MidiPermissionContext to control the display and handling of MIDI permission
// infobars to the user.
class MidiPermissionInfoBarDelegateAndroid : public PermissionInfoBarDelegate {
 public:
  MidiPermissionInfoBarDelegateAndroid(const GURL& requesting_frame,
                                       bool user_gesture,
                                       Profile* profile,
                                       const PermissionSetCallback& callback);

 private:
  ~MidiPermissionInfoBarDelegateAndroid() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  int GetMessageResourceId() const override;

  DISALLOW_COPY_AND_ASSIGN(MidiPermissionInfoBarDelegateAndroid);
};

#endif  // CHROME_BROWSER_MEDIA_MIDI_PERMISSION_INFOBAR_DELEGATE_ANDROID_H_
