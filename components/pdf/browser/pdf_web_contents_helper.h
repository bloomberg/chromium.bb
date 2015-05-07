// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
#define COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"

namespace content {
class WebContents;
}

namespace pdf {

class OpenPDFInReaderPromptClient;
class PDFWebContentsHelperClient;

// Per-WebContents class to handle PDF messages.
class PDFWebContentsHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PDFWebContentsHelper> {
 public:
  static void CreateForWebContentsWithClient(
      content::WebContents* contents,
      scoped_ptr<PDFWebContentsHelperClient> client);

  OpenPDFInReaderPromptClient* open_in_reader_prompt() const {
    return open_in_reader_prompt_.get();
  }

  void ShowOpenInReaderPrompt(scoped_ptr<OpenPDFInReaderPromptClient> prompt);

 private:
  PDFWebContentsHelper(content::WebContents* web_contents,
                       scoped_ptr<PDFWebContentsHelperClient> client);
  ~PDFWebContentsHelper() override;

  // content::WebContentsObserver overrides:
  bool OnMessageReceived(const IPC::Message& message) override;
  void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) override;

  // Internal helpers ----------------------------------------------------------

  void UpdateLocationBar();

  // Message handlers.
  void OnHasUnsupportedFeature();
  void OnSaveURLAs(const GURL& url, const content::Referrer& referrer);
  void OnUpdateContentRestrictions(int content_restrictions);

  // The model for the confirmation prompt to open a PDF in Adobe Reader.
  scoped_ptr<OpenPDFInReaderPromptClient> open_in_reader_prompt_;
  scoped_ptr<PDFWebContentsHelperClient> client_;

  DISALLOW_COPY_AND_ASSIGN(PDFWebContentsHelper);
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_WEB_CONTENTS_HELPER_H_
