// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/sys_string_conversions.h"

bool CanShowNetworkDiagnosticsDialog() {
  return true;
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const std::string& failed_url) {
  base::ScopedCFTypeRef<CFStringRef> url_string_ref(
      base::SysUTF8ToCFStringRef(failed_url));
  base::ScopedCFTypeRef<CFURLRef> url_ref(
      CFURLCreateWithString(kCFAllocatorDefault, url_string_ref.get(),
                            nullptr));
  if (!url_ref.get())
    return;

  base::ScopedCFTypeRef<CFNetDiagnosticRef> diagnostic_ref(
      CFNetDiagnosticCreateWithURL(kCFAllocatorDefault, url_ref.get()));
  if (!diagnostic_ref.get())
    return;

  CFNetDiagnosticDiagnoseProblemInteractively(diagnostic_ref.get());
}
