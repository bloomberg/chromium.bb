// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_
#pragma once

#include "base/values.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"

// The WebUI for chrome://view-cert
class CertificateViewerUI : public HtmlDialogUI {
 public:
  explicit CertificateViewerUI(TabContents* contents);
  virtual ~CertificateViewerUI();

 protected:
  // Extracts the certificate details and returns them to the javascript
  // function getCertificateInfo in a dictionary structure.
  //
  // The input is an X509Certificate pointer in hex encoded format in the first
  // argument of the args list.
  void RequestCertificateInfo(const ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(CertificateViewerUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATE_VIEWER_UI_H_
