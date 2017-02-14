// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/public_session_tab_capture_access_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "chromeos/login/login_state.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/manifest_permission_set.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern_set.h"

namespace {

// Returns true if we're in a Public Session.
bool IsPublicSession() {
  return chromeos::LoginState::IsInitialized() &&
         chromeos::LoginState::Get()->IsPublicSessionUser();
}

}  // namespace

PublicSessionTabCaptureAccessHandler::PublicSessionTabCaptureAccessHandler() {}

PublicSessionTabCaptureAccessHandler::~PublicSessionTabCaptureAccessHandler() {}

bool PublicSessionTabCaptureAccessHandler::SupportsStreamType(
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return tab_capture_access_handler_.SupportsStreamType(type, extension);
}

bool PublicSessionTabCaptureAccessHandler::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return tab_capture_access_handler_.CheckMediaAccessPermission(
      web_contents, security_origin, type, extension);
}

void PublicSessionTabCaptureAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  // This class handles requests for Public Sessions only, outside of them just
  // pass the request through to the original class.
  if (!IsPublicSession() || !extension) {
    return tab_capture_access_handler_.HandleRequest(web_contents, request,
                                                     callback, extension);
  }

  UserChoice& user_choice = user_choice_cache_[extension->id()];

  if ((request.audio_type != content::MEDIA_TAB_AUDIO_CAPTURE &&
       request.video_type != content::MEDIA_TAB_VIDEO_CAPTURE) ||
      !user_choice.NeedsPrompting()) {
    return ChainHandleRequest(web_contents, request, callback, extension);
  }

  user_choice.SetPrompted();

  extensions::APIPermissionSet new_apis;
  new_apis.insert(extensions::APIPermission::kTabCapture);
  auto permission_set = base::MakeUnique<extensions::PermissionSet>(
      new_apis, extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(), extensions::URLPatternSet());
  auto prompt = base::MakeUnique<ExtensionInstallPrompt>(web_contents);

  prompt->ShowDialog(
      base::Bind(&PublicSessionTabCaptureAccessHandler::ResolvePermissionPrompt,
                 base::Unretained(this), web_contents, request, callback,
                 extension),
      extension,
      nullptr,  // Uses the extension icon.
      base::MakeUnique<ExtensionInstallPrompt::Prompt>(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT),
      std::move(permission_set),
      ExtensionInstallPrompt::GetDefaultShowDialogCallback());

  extension_install_prompt_map_[extension->id()] = std::move(prompt);
}

void PublicSessionTabCaptureAccessHandler::ChainHandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK(IsPublicSession() && extension);
  const UserChoice& user_choice = user_choice_cache_[extension->id()];
  content::MediaStreamRequest request_copy(request);

  // If the user denied tab capture, here the request gets filtered out before
  // being passed on to the actual implementation.
  if (!user_choice.IsAllowed()) {
    request_copy.audio_type = content::MEDIA_NO_SERVICE;
    request_copy.video_type = content::MEDIA_NO_SERVICE;
  }

  // Pass the request through to the original class.
  tab_capture_access_handler_.HandleRequest(web_contents, request_copy,
                                            callback, extension);
}

void PublicSessionTabCaptureAccessHandler::ResolvePermissionPrompt(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension,
    ExtensionInstallPrompt::Result prompt_result) {
  // Dispose of the prompt as it's not needed anymore.
  extension_install_prompt_map_.erase(extension->id());

  bool allowed = prompt_result == ExtensionInstallPrompt::Result::ACCEPTED;
  UserChoice& user_choice = user_choice_cache_[extension->id()];

  user_choice.Set(allowed);

  ChainHandleRequest(web_contents, request, callback, extension);
}

bool PublicSessionTabCaptureAccessHandler::UserChoice::IsAllowed() const {
  return tab_capture_allowed_;
}

bool PublicSessionTabCaptureAccessHandler::UserChoice::NeedsPrompting() const {
  return !tab_capture_prompted_;
}

void PublicSessionTabCaptureAccessHandler::UserChoice::Set(bool allowed) {
  tab_capture_allowed_ = allowed;
}

void PublicSessionTabCaptureAccessHandler::UserChoice::SetPrompted() {
  tab_capture_prompted_ = true;
}
