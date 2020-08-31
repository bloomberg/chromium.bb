// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"

#include "chrome/browser/sharing/features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace qrcode_generator {

QRCodeGeneratorBubbleController::~QRCodeGeneratorBubbleController() {
  HideBubble();
}

// static
bool QRCodeGeneratorBubbleController::IsGeneratorAvailable(const GURL& url,
                                                           bool in_incognito) {
  if (in_incognito)
    return false;

  if (!base::FeatureList::IsEnabled(kSharingQRCodeGenerator))
    return false;

  if (!url.SchemeIsHTTPOrHTTPS())
    return false;

  return true;
}

// static
QRCodeGeneratorBubbleController* QRCodeGeneratorBubbleController::Get(
    content::WebContents* web_contents) {
  QRCodeGeneratorBubbleController::CreateForWebContents(web_contents);
  QRCodeGeneratorBubbleController* controller =
      QRCodeGeneratorBubbleController::FromWebContents(web_contents);
  return controller;
}

void QRCodeGeneratorBubbleController::ShowBubble(const GURL& url) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  qrcode_generator_bubble_ =
      browser->window()->ShowQRCodeGeneratorBubble(web_contents_, this, url);

  browser->window()->UpdatePageActionIcon(PageActionIconType::kQRCodeGenerator);
}

void QRCodeGeneratorBubbleController::HideBubble() {
  if (qrcode_generator_bubble_) {
    qrcode_generator_bubble_->Hide();
    qrcode_generator_bubble_ = nullptr;
  }
}

QRCodeGeneratorBubbleView*
QRCodeGeneratorBubbleController::qrcode_generator_bubble_view() const {
  return qrcode_generator_bubble_;
}

void QRCodeGeneratorBubbleController::OnBubbleClosed() {
  qrcode_generator_bubble_ = nullptr;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents_);
  browser->window()->UpdatePageActionIcon(PageActionIconType::kQRCodeGenerator);
}

QRCodeGeneratorBubbleController::QRCodeGeneratorBubbleController() = default;

QRCodeGeneratorBubbleController::QRCodeGeneratorBubbleController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(QRCodeGeneratorBubbleController)

}  // namespace qrcode_generator
