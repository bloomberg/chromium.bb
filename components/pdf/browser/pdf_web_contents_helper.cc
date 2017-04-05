// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_web_contents_helper.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(pdf::PDFWebContentsHelper);

namespace pdf {

// static
void PDFWebContentsHelper::CreateForWebContentsWithClient(
    content::WebContents* contents,
    std::unique_ptr<PDFWebContentsHelperClient> client) {
  if (FromWebContents(contents))
    return;
  contents->SetUserData(UserDataKey(),
                        new PDFWebContentsHelper(contents, std::move(client)));
}

PDFWebContentsHelper::PDFWebContentsHelper(
    content::WebContents* web_contents,
    std::unique_ptr<PDFWebContentsHelperClient> client)
    : content::WebContentsObserver(web_contents),
      pdf_service_bindings_(web_contents, this),
      client_(std::move(client)) {}

PDFWebContentsHelper::~PDFWebContentsHelper() {
}

void PDFWebContentsHelper::HasUnsupportedFeature() {
  client_->OnPDFHasUnsupportedFeature(web_contents());
}

void PDFWebContentsHelper::SaveUrlAs(const GURL& url,
                                     const content::Referrer& referrer) {
  client_->OnSaveURL(web_contents());
  web_contents()->SaveFrame(url, referrer);
}

void PDFWebContentsHelper::UpdateContentRestrictions(
    int32_t content_restrictions) {
  client_->UpdateContentRestrictions(web_contents(), content_restrictions);
}

}  // namespace pdf
