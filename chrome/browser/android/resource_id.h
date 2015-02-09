// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_RESOURCE_ID_H_
#define CHROME_BROWSER_ANDROID_RESOURCE_ID_H_

// This file maps Chromium resource IDs to Android resource IDs.
#ifndef DEFINE_RESOURCE_ID
#error "DEFINE_RESOURCE_ID should be defined before including this file"
#endif

// Create a mapping that identifies when a resource isn't being passed in.
DEFINE_RESOURCE_ID(0, 0)

// InfoBar resources.
DEFINE_RESOURCE_ID(IDR_INFOBAR_AUTOFILL_CC, R.drawable.infobar_autofill_cc)
DEFINE_RESOURCE_ID(IDR_INFOBAR_AUTOLOGIN,\
                   R.drawable.infobar_savepassword_autologin)
DEFINE_RESOURCE_ID(IDR_INFOBAR_DESKTOP_NOTIFICATIONS,\
                   R.drawable.infobar_desktop_notifications)
DEFINE_RESOURCE_ID(IDR_INFOBAR_GEOLOCATION, R.drawable.infobar_geolocation)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MEDIA_STREAM_CAMERA, R.drawable.infobar_camera)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MEDIA_STREAM_MIC, R.drawable.infobar_microphone)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MIDI, R.drawable.infobar_midi)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MULTIPLE_DOWNLOADS,\
                   R.drawable.infobar_multiple_downloads)
DEFINE_RESOURCE_ID(IDR_INFOBAR_PROTECTED_MEDIA_IDENTIFIER,
                   R.drawable.infobar_protected_media_identifier)
DEFINE_RESOURCE_ID(IDR_INFOBAR_SAVE_PASSWORD,\
                   R.drawable.infobar_savepassword_autologin)
DEFINE_RESOURCE_ID(IDR_INFOBAR_WARNING, R.drawable.infobar_warning)
DEFINE_RESOURCE_ID(IDR_INFOBAR_TRANSLATE, R.drawable.infobar_translate)
DEFINE_RESOURCE_ID(IDR_BLOCKED_POPUPS, R.drawable.infobar_blocked_popups)

// Autofill popup images.
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_AMEX, R.drawable.amex_card)
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_DISCOVER, R.drawable.discover_card)
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_GENERIC, R.drawable.generic_card)
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_MASTERCARD, R.drawable.mc_card)
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_VISA, R.drawable.visa_card)
DEFINE_RESOURCE_ID(IDR_AUTOFILL_CC_SCAN_NEW, android.R.drawable.ic_menu_camera)
DEFINE_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT, R.drawable.cvc_icon)
DEFINE_RESOURCE_ID(IDR_CREDIT_CARD_CVC_HINT_AMEX, R.drawable.cvc_icon_amex)

#endif  // CHROME_BROWSER_ANDROID_RESOURCE_ID_H_
