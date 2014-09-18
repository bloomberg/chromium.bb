// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/shell/athena_shell_app_delegate.h"

#include "content/public/browser/web_contents.h"
#include "extensions/shell/browser/media_capture_util.h"

namespace athena {

AthenaShellAppDelegate::AthenaShellAppDelegate() {
}

AthenaShellAppDelegate::~AthenaShellAppDelegate() {
}

void AthenaShellAppDelegate::InitWebContents(
    content::WebContents* web_contents) {
  // TODO(oshima): Enable Favicon, Printing, e c. See
  // athena_chrome_app_delegate.cc.
  NOTIMPLEMENTED();
}

content::ColorChooser* AthenaShellAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  NOTIMPLEMENTED();
  return NULL;
}

void AthenaShellAppDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  NOTIMPLEMENTED();
}

void AthenaShellAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  extensions::media_capture_util::GrantMediaStreamRequest(
      web_contents, request, callback, extension);
}

bool AthenaShellAppDelegate::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  extensions::media_capture_util::VerifyMediaAccessPermission(type, extension);
  return true;
}

void AthenaShellAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  NOTIMPLEMENTED();
}
}  // namespace athena
