// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pdf/browser/open_pdf_in_reader_prompt_client.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"
#include "components/pdf/common/pdf_messages.h"
#include "content/public/browser/navigation_details.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(pdf::PDFWebContentsHelper);

namespace pdf {

// static
void PDFWebContentsHelper::CreateForWebContentsWithClient(
    content::WebContents* contents,
    scoped_ptr<PDFWebContentsHelperClient> client) {
  if (FromWebContents(contents))
    return;
  contents->SetUserData(UserDataKey(),
                        new PDFWebContentsHelper(contents, client.Pass()));
}

PDFWebContentsHelper::PDFWebContentsHelper(
    content::WebContents* web_contents,
    scoped_ptr<PDFWebContentsHelperClient> client)
    : content::WebContentsObserver(web_contents), client_(client.Pass()) {
}

PDFWebContentsHelper::~PDFWebContentsHelper() {
}

void PDFWebContentsHelper::ShowOpenInReaderPrompt(
    scoped_ptr<OpenPDFInReaderPromptClient> prompt) {
  open_in_reader_prompt_ = prompt.Pass();
  UpdateLocationBar();
}

bool PDFWebContentsHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PDFWebContentsHelper, message)
    IPC_MESSAGE_HANDLER(PDFHostMsg_PDFHasUnsupportedFeature,
                        OnHasUnsupportedFeature)
    IPC_MESSAGE_HANDLER(PDFHostMsg_PDFSaveURLAs, OnSaveURLAs)
    IPC_MESSAGE_HANDLER(PDFHostMsg_PDFUpdateContentRestrictions,
                        OnUpdateContentRestrictions)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(PDFHostMsg_PDFModalPromptForPassword,
                                    OnModalPromptForPassword)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PDFWebContentsHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (open_in_reader_prompt_.get() &&
      open_in_reader_prompt_->ShouldExpire(details)) {
    open_in_reader_prompt_.reset();
    UpdateLocationBar();
  }
}

void PDFWebContentsHelper::UpdateLocationBar() {
  client_->UpdateLocationBar(web_contents());
}

void PDFWebContentsHelper::OnHasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(web_contents());
}

void PDFWebContentsHelper::OnSaveURLAs(const GURL& url,
                                       const content::Referrer& referrer) {
  client_->OnSaveURL(web_contents());
  web_contents()->SaveFrame(url, referrer);
}

void PDFWebContentsHelper::OnUpdateContentRestrictions(
    int content_restrictions) {
  client_->UpdateContentRestrictions(web_contents(), content_restrictions);
}

void PDFWebContentsHelper::OnModalPromptForPasswordClosed(
    IPC::Message* reply_message,
    bool success,
    const base::string16& actual_value) {
  PDFHostMsg_PDFModalPromptForPassword::WriteReplyParams(
      reply_message, base::UTF16ToUTF8(actual_value));
  Send(reply_message);
}

void PDFWebContentsHelper::OnModalPromptForPassword(
    const std::string& prompt,
    IPC::Message* reply_message) {
  base::Callback<void(bool, const base::string16&)> callback =
      base::Bind(&PDFWebContentsHelper::OnModalPromptForPasswordClosed,
                 base::Unretained(this),
                 reply_message);
  client_->OnShowPDFPasswordDialog(
      web_contents(), base::UTF8ToUTF16(prompt), callback);
}

}  // namespace pdf
