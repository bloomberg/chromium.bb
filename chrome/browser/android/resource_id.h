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
DEFINE_RESOURCE_ID(IDR_PROTECTED_MEDIA_IDENTIFIER_INFOBAR_ICON,
                   R.drawable.infobar_protected_media_identifier)
DEFINE_RESOURCE_ID(IDR_GEOLOCATION_INFOBAR_ICON, R.drawable.infobar_geolocation)
DEFINE_RESOURCE_ID(IDR_INFOBAR_ALT_NAV_URL, R.drawable.infobar_didyoumean)
DEFINE_RESOURCE_ID(IDR_INFOBAR_AUTOFILL, R.drawable.infobar_autofill)
DEFINE_RESOURCE_ID(IDR_INFOBAR_AUTOLOGIN, R.drawable.infobar_autologin)
DEFINE_RESOURCE_ID(IDR_INFOBAR_COOKIE, R.drawable.infobar_cookie)
DEFINE_RESOURCE_ID(IDR_INFOBAR_DESKTOP_NOTIFICATIONS,\
                   R.drawable.infobar_desktop_notifications)
DEFINE_RESOURCE_ID(IDR_INFOBAR_INCOMPLETE, R.drawable.infobar_incomplete)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MEDIA_STREAM_CAMERA, R.drawable.infobar_camera)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MEDIA_STREAM_MIC, R.drawable.infobar_microphone)
DEFINE_RESOURCE_ID(IDR_INFOBAR_MULTIPLE_DOWNLOADS,\
                   R.drawable.infobar_multiple_downloads)
DEFINE_RESOURCE_ID(IDR_INFOBAR_PLUGIN_CRASHED,\
                   R.drawable.infobar_plugin_crashed)
DEFINE_RESOURCE_ID(IDR_INFOBAR_PLUGIN_INSTALL, R.drawable.infobar_plugin)
DEFINE_RESOURCE_ID(IDR_INFOBAR_RESTORE_SESSION, R.drawable.infobar_restore)
DEFINE_RESOURCE_ID(IDR_INFOBAR_SAVE_PASSWORD, R.drawable.infobar_savepassword)
DEFINE_RESOURCE_ID(IDR_INFOBAR_WARNING, R.drawable.infobar_warning)
DEFINE_RESOURCE_ID(IDR_INFOBAR_THEME, R.drawable.infobar_theme)
DEFINE_RESOURCE_ID(IDR_INFOBAR_TRANSLATE, R.drawable.infobar_translate)

// WebsiteSettingsUI images.
DEFINE_RESOURCE_ID(IDR_CONTROLLED_SETTING_MANDATORY_LARGE,\
                   R.drawable.controlled_setting_mandatory_large)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_BAD, R.drawable.pageinfo_bad)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_GOOD, R.drawable.pageinfo_good)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_INFO, R.drawable.pageinfo_info)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MAJOR,\
                   R.drawable.pageinfo_warning_major)
DEFINE_RESOURCE_ID(IDR_PAGEINFO_WARNING_MINOR,\
                   R.drawable.pageinfo_warning_minor)

#endif  // CHROME_BROWSER_ANDROID_RESOURCE_ID_H_
