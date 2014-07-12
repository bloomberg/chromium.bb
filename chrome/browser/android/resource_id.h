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

// WebsiteSettingsUI images.
DEFINE_RESOURCE_ID(IDR_PAGEINFO_ENTERPRISE_MANAGED,\
                   R.drawable.pageinfo_enterprise_managed)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_BAD, R.drawable.pageinfo_bad)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_GOOD, R.drawable.pageinfo_good)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_INFO, R.drawable.pageinfo_info)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MAJOR,\
                   R.drawable.pageinfo_warning_major)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MINOR,\
                   R.drawable.pageinfo_warning_minor)

#endif  // CHROME_BROWSER_ANDROID_RESOURCE_ID_H_
