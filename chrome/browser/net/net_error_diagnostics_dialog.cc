// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_error_diagnostics_dialog.h"

#include "base/logging.h"

#if defined(OS_MACOSX)
#include "chrome/browser/net/net_error_diagnostics_dialog_mac.h"
#endif

bool CanShowNetworkDiagnosticsDialog() {
#if defined(OS_MACOSX)
  return true;
#else
  return false;
#endif
}

void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const GURL& failed_url) {
  DCHECK(CanShowNetworkDiagnosticsDialog());
#if defined(OS_MACOSX)
  ShowNetworkDiagnosticsDialogMac(web_contents, failed_url);
#else
  NOTREACHED();
#endif
}
