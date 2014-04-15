// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ipc/ipc_message.h"

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
  void OnModalPromptForPasswordClosed(IPC::Message* reply_message,
                                      bool success,
                                      const base::string16& actual_value);

  // Message handlers.
  void OnHasUnsupportedFeature();
  void OnSaveURLAs(const GURL& url,
                   const content::Referrer& referrer);
  void OnUpdateContentRestrictions(int content_restrictions);
  void OnModalPromptForPassword(const std::string& prompt,
                                IPC::Message* reply_message);

  // The model for the confirmation prompt to open a PDF in Adobe Reader.
  scoped_ptr<OpenPDFInReaderPromptDelegate> open_in_reader_prompt_;

  DISALLOW_COPY_AND_ASSIGN(PDFTabHelper);
};

typedef base::Callback<void(bool /* success */,
                            const base::string16& /* password */)>
                                PasswordDialogClosedCallback;

// Shows a tab-modal dialog to get a password for a PDF document.
void ShowPDFPasswordDialog(content::WebContents* web_contents,
                           const base::string16& prompt,
                           const PasswordDialogClosedCallback& callback);

#endif  // CHROME_BROWSER_UI_PDF_PDF_TAB_HELPER_H_
