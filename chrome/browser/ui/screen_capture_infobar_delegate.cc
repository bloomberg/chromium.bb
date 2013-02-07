// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/screen_capture_infobar_delegate.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
void ScreenCaptureInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback) {
  bool screen_capture_enabled = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUserMediaScreenCapturing);
  // Deny request automatically in the following cases:
  //  1. Screen capturing is not enabled via command line switch.
  //  2. Audio capture was requested (it's not supported yet).
  //  3. Request from a page that was not loaded from a secure origin.
  if (!screen_capture_enabled ||
      request.audio_type != content::MEDIA_NO_SERVICE ||
      !request.security_origin.SchemeIsSecure()) {
    callback.Run(content::MediaStreamDevices());
    return;
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new ScreenCaptureInfoBarDelegate(infobar_service, request, callback)));
}

ScreenCaptureInfoBarDelegate::ScreenCaptureInfoBarDelegate(
    InfoBarService* infobar_service,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback)
    : ConfirmInfoBarDelegate(infobar_service),
      request_(request),
      callback_(callback) {
  DCHECK_EQ(content::MEDIA_SCREEN_VIDEO_CAPTURE, request.video_type);
}

ScreenCaptureInfoBarDelegate::~ScreenCaptureInfoBarDelegate() {
}

// Needed to avoid having more than one infobar with the same request.
bool ScreenCaptureInfoBarDelegate::EqualsDelegate(
    InfoBarDelegate* delegate) const {
  ScreenCaptureInfoBarDelegate* other =
      delegate->AsScreenCaptureInfoBarDelegate();
  return other && other->request_.security_origin == request_.security_origin;
}

void ScreenCaptureInfoBarDelegate::InfoBarDismissed() {
  Deny();
}

InfoBarDelegate::Type ScreenCaptureInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

ScreenCaptureInfoBarDelegate*
    ScreenCaptureInfoBarDelegate::AsScreenCaptureInfoBarDelegate() {
  return this;
}

string16 ScreenCaptureInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_MEDIA_CAPTURE_SCREEN,
      UTF8ToUTF16(request_.security_origin.spec()));
}

string16 ScreenCaptureInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_MEDIA_CAPTURE_ALLOW : IDS_MEDIA_CAPTURE_DENY);
}

bool ScreenCaptureInfoBarDelegate::Accept() {
  content::MediaStreamDevices devices;

  // Add screen capturer source if it was requested.
  devices.push_back(content::MediaStreamDevice(
      content::MEDIA_SCREEN_VIDEO_CAPTURE, std::string(), "Screen"));

  callback_.Run(devices);
  return true;
}

bool ScreenCaptureInfoBarDelegate::Cancel() {
  Deny();
  return true;
}

void ScreenCaptureInfoBarDelegate::Deny() {
  callback_.Run(content::MediaStreamDevices());
}
