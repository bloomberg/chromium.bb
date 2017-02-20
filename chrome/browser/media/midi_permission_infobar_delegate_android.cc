// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_permission_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"

MidiPermissionInfoBarDelegateAndroid::MidiPermissionInfoBarDelegateAndroid(
    const GURL& requesting_frame,
    bool user_gesture,
    Profile* profile,
    const PermissionSetCallback& callback)
    : PermissionInfoBarDelegate(requesting_frame,
                                CONTENT_SETTINGS_TYPE_MIDI_SYSEX,
                                user_gesture,
                                profile,
                                callback) {}

MidiPermissionInfoBarDelegateAndroid::~MidiPermissionInfoBarDelegateAndroid() {}

infobars::InfoBarDelegate::InfoBarIdentifier
MidiPermissionInfoBarDelegateAndroid::GetIdentifier() const {
  return MIDI_PERMISSION_INFOBAR_DELEGATE_ANDROID;
}

int MidiPermissionInfoBarDelegateAndroid::GetIconId() const {
  return IDR_ANDROID_INFOBAR_MIDI;
}

int MidiPermissionInfoBarDelegateAndroid::GetMessageResourceId() const {
  return IDS_MIDI_SYSEX_INFOBAR_QUESTION;
}
