// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_MAC_H_
#define CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_MAC_H_

class GURL;

namespace content {
class WebContents;
}

void ShowNetworkDiagnosticsDialogMac(content::WebContents* web_contents,
                                     const GURL& failed_url);

#endif  // CHROME_BROWSER_NET_NET_ERROR_DIAGNOSTICS_DIALOG_MAC_H_

