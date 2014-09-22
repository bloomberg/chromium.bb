// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_delegate.h"

#include "extensions/common/constants.h"
#include "extensions/shell/browser/media_capture_util.h"

namespace extensions {

ShellAppDelegate::ShellAppDelegate() {
}

ShellAppDelegate::~ShellAppDelegate() {
}

void ShellAppDelegate::InitWebContents(content::WebContents* web_contents) {
}

void ShellAppDelegate::ResizeWebContents(content::WebContents* web_contents,
                                         const gfx::Size& size) {
  NOTIMPLEMENTED();
}

content::WebContents* ShellAppDelegate::OpenURLFromTab(
    content::BrowserContext* context,
    content::WebContents* source,
    const content::OpenURLParams& params) {
  NOTIMPLEMENTED();
  return NULL;
}

void ShellAppDelegate::AddNewContents(content::BrowserContext* context,
                                      content::WebContents* new_contents,
                                      WindowOpenDisposition disposition,
                                      const gfx::Rect& initial_pos,
                                      bool user_gesture,
                                      bool* was_blocked) {
  NOTIMPLEMENTED();
}

content::ColorChooser* ShellAppDelegate::ShowColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  NOTIMPLEMENTED();
  return NULL;
}

void ShellAppDelegate::RunFileChooser(
    content::WebContents* tab,
    const content::FileChooserParams& params) {
  NOTIMPLEMENTED();
}

void ShellAppDelegate::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  media_capture_util::GrantMediaStreamRequest(
      web_contents, request, callback, extension);
}

bool ShellAppDelegate::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const Extension* extension) {
  media_capture_util::VerifyMediaAccessPermission(type, extension);
  return true;
}

int ShellAppDelegate::PreferredIconSize() {
  return extension_misc::EXTENSION_ICON_SMALL;
}

void ShellAppDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents,
    bool blocked) {
  NOTIMPLEMENTED();
}

bool ShellAppDelegate::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return true;
}

void ShellAppDelegate::SetTerminatingCallback(const base::Closure& callback) {
  // TODO(jamescook): Should app_shell continue to close the app window
  // manually or should it use a browser termination callback like Chrome?
  NOTIMPLEMENTED();
}

}  // namespace extensions
