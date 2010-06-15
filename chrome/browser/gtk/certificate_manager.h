// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_CERTIFICATE_MANAGER_H_
#define CHROME_BROWSER_GTK_CERTIFICATE_MANAGER_H_

#include "gfx/native_widget_types.h"

class Profile;
class PrefService;

// An identifier for the Certificate Manager window. These are treated as
// indices into the list of available tabs to be displayed.
// PAGE_DEFAULT means select the last tab viewed when the Certificate Manager
// window was opened, or PAGE_USER if the Certificate Manager was never opened.
enum CertificateManagerPage {
  PAGE_DEFAULT = -1,
  PAGE_USER,
  PAGE_EMAIL,
  PAGE_SERVER,
  PAGE_CA,
  PAGE_UNKNOWN,
  PAGE_COUNT
};

namespace certificate_manager_util {

void RegisterUserPrefs(PrefService* prefs);

}  // namespace certificate_manager_util

void ShowCertificateManager(gfx::NativeWindow parent, Profile* profile,
                            CertificateManagerPage page);

#endif  // CHROME_BROWSER_GTK_CERTIFICATE_MANAGER_H_
