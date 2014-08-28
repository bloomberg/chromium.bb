// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
#define CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_

#include "base/macros.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"

class ChromePDFWebContentsHelperClient
    : public pdf::PDFWebContentsHelperClient {
 public:
  ChromePDFWebContentsHelperClient();
  virtual ~ChromePDFWebContentsHelperClient();

 private:
  // pdf::PDFWebContentsHelperClient:
  virtual void UpdateLocationBar(content::WebContents* contents) OVERRIDE;

  virtual void UpdateContentRestrictions(content::WebContents* contents,
                                         int content_restrictions) OVERRIDE;

  virtual void OnPDFHasUnsupportedFeature(
      content::WebContents* contents) OVERRIDE;

  virtual void OnSaveURL(content::WebContents* contents) OVERRIDE;

  virtual void OnShowPDFPasswordDialog(
      content::WebContents* contents,
      const base::string16& prompt,
      const pdf::PasswordDialogClosedCallback& callback) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromePDFWebContentsHelperClient);
};

void ShowPDFPasswordDialog(content::WebContents* web_contents,
                           const base::string16& prompt,
                           const pdf::PasswordDialogClosedCallback& callback);

#endif  // CHROME_BROWSER_UI_PDF_CHROME_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
