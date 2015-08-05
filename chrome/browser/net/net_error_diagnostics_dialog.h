// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_
#define CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_

class GURL;

namespace content {
class WebContents;
}

// Returns true if the platform has a supported tool for diagnosing network
// errors encountered when requesting URLs.
bool CanShowNetworkDiagnosticsDialog();

// Shows a dialog for investigating an error received when requesting
// |failed_url|.  May only be called when CanShowNetworkDiagnosticsDialog()
// returns true.
void ShowNetworkDiagnosticsDialog(content::WebContents* web_contents,
                                  const GURL& failed_url);

#endif  // CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_H_

