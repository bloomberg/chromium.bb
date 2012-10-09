// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class OpenPDFInReaderPromptDelegate;

namespace content {
class WebContents;
}

// Per-tab class to handle PDF messages.
class PDFTabHelper : public content::WebContentsObserver,
                     public content::WebContentsUserData<PDFTabHelper>  {
 public:

  explicit PDFTabHelper(content::WebContents* web_contents);
  virtual ~PDFTabHelper();

  OpenPDFInReaderPromptDelegate* open_in_reader_prompt() const {
    return open_in_reader_prompt_.get();
  }

  void ShowOpenInReaderPrompt(scoped_ptr<OpenPDFInReaderPromptDelegate> prompt);

 private:
  // content::WebContentsObserver overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  void UpdateLocationBar();

  // Message handlers.
  void OnPDFHasUnsupportedFeature();

  // The model for the confirmation prompt to open a PDF in Adobe Reader.
  scoped_ptr<OpenPDFInReaderPromptDelegate> open_in_reader_prompt_;

  DISALLOW_COPY_AND_ASSIGN(PDFTabHelper);
};

#endif  // CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_
