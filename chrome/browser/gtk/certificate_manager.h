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
// CERT_MANAGER_PAGE_DEFAULT means select the last tab viewed when the
// Certificate Manager window was opened, or CERT_MANAGER_PAGE_USER if the
// Certificate Manager was never opened.
enum CertificateManagerPage {
  CERT_MANAGER_PAGE_DEFAULT = -1,
  CERT_MANAGER_PAGE_USER,
  CERT_MANAGER_PAGE_EMAIL,
  CERT_MANAGER_PAGE_SERVER,
  CERT_MANAGER_PAGE_CA,
  CERT_MANAGER_PAGE_UNKNOWN,
  CERT_MANAGER_PAGE_COUNT
};

namespace certificate_manager_util {

void RegisterUserPrefs(PrefService* prefs);

}  // namespace certificate_manager_util

void ShowCertificateManager(gfx::NativeWindow parent, Profile* profile,
                            CertificateManagerPage page);

#endif  // CHROME_BROWSER_GTK_CERTIFICATE_MANAGER_H_
