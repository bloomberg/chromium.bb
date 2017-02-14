// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/public_session_media_access_handler.h"

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

PublicSessionMediaAccessHandler::PublicSessionMediaAccessHandler() {}

PublicSessionMediaAccessHandler::~PublicSessionMediaAccessHandler() {}

bool PublicSessionMediaAccessHandler::SupportsStreamType(
    const content::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension_media_access_handler_.SupportsStreamType(type, extension);
}

bool PublicSessionMediaAccessHandler::CheckMediaAccessPermission(
    content::WebContents* web_contents,
    const GURL& security_origin,
    content::MediaStreamType type,
    const extensions::Extension* extension) {
  return extension_media_access_handler_.CheckMediaAccessPermission(
      web_contents, security_origin, type, extension);
}

void PublicSessionMediaAccessHandler::HandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  // This class handles requests for Public Sessions only, outside of them just
  // pass the request through to the original class.
  if (!IsPublicSession() || !extension->is_platform_app()) {
    return extension_media_access_handler_.HandleRequest(web_contents, request,
                                                         callback, extension);
  }

  bool needs_prompt = false;
  extensions::APIPermissionSet new_apis;
  UserChoice& user_choice = user_choice_cache_[extension->id()];

  if (user_choice.NeedsPrompting(request.audio_type)) {
    new_apis.insert(extensions::APIPermission::kAudioCapture);
    user_choice.SetPrompted(content::MEDIA_DEVICE_AUDIO_CAPTURE);
    needs_prompt = true;
  }
  if (user_choice.NeedsPrompting(request.video_type)) {
    new_apis.insert(extensions::APIPermission::kVideoCapture);
    user_choice.SetPrompted(content::MEDIA_DEVICE_VIDEO_CAPTURE);
    needs_prompt = true;
  }

  if (!needs_prompt)
    return ChainHandleRequest(web_contents, request, callback, extension);

  auto permission_set = base::MakeUnique<extensions::PermissionSet>(
      new_apis, extensions::ManifestPermissionSet(),
      extensions::URLPatternSet(), extensions::URLPatternSet());

  auto prompt = base::MakeUnique<ExtensionInstallPrompt>(web_contents);

  prompt->ShowDialog(
      base::Bind(&PublicSessionMediaAccessHandler::ResolvePermissionPrompt,
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

void PublicSessionMediaAccessHandler::ChainHandleRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension) {
  DCHECK(IsPublicSession() && extension && extension->is_platform_app());
  const UserChoice& user_choice = user_choice_cache_[extension->id()];
  content::MediaStreamRequest request_copy(request);

  // If the user denies audio or video capture, here it gets filtered out from
  // the request before being passed on to the actual implementation.
  if (!user_choice.IsAllowed(content::MEDIA_DEVICE_AUDIO_CAPTURE))
    request_copy.audio_type = content::MEDIA_NO_SERVICE;
  if (!user_choice.IsAllowed(content::MEDIA_DEVICE_VIDEO_CAPTURE))
    request_copy.video_type = content::MEDIA_NO_SERVICE;

  // Pass the request through to the original class.
  extension_media_access_handler_.HandleRequest(web_contents, request_copy,
                                                callback, extension);
}

void PublicSessionMediaAccessHandler::ResolvePermissionPrompt(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    const content::MediaResponseCallback& callback,
    const extensions::Extension* extension,
    ExtensionInstallPrompt::Result prompt_result) {
  // Dispose of the prompt as it's not needed anymore.
  extension_install_prompt_map_.erase(extension->id());

  bool allowed = prompt_result == ExtensionInstallPrompt::Result::ACCEPTED;
  UserChoice& user_choice = user_choice_cache_[extension->id()];

  if (request.audio_type == content::MEDIA_DEVICE_AUDIO_CAPTURE)
    user_choice.Set(content::MEDIA_DEVICE_AUDIO_CAPTURE, allowed);
  if (request.video_type == content::MEDIA_DEVICE_VIDEO_CAPTURE)
    user_choice.Set(content::MEDIA_DEVICE_VIDEO_CAPTURE, allowed);

  ChainHandleRequest(web_contents, request, callback, extension);
}

bool PublicSessionMediaAccessHandler::UserChoice::IsAllowed(
    content::MediaStreamType type) const {
  switch (type) {
    case content::MEDIA_DEVICE_AUDIO_CAPTURE:
      return !audio_prompted_ || audio_allowed_;
    case content::MEDIA_DEVICE_VIDEO_CAPTURE:
      return !video_prompted_ || video_allowed_;
    default:
      NOTREACHED();
      return false;
  }
}

bool PublicSessionMediaAccessHandler::UserChoice::NeedsPrompting(
    content::MediaStreamType type) const {
  switch (type) {
    case content::MEDIA_DEVICE_AUDIO_CAPTURE:
      return !audio_prompted_;
    case content::MEDIA_DEVICE_VIDEO_CAPTURE:
      return !video_prompted_;
    default:
      return false;
  }
}

void PublicSessionMediaAccessHandler::UserChoice::Set(
    content::MediaStreamType type,
    bool allowed) {
  switch (type) {
    case content::MEDIA_DEVICE_AUDIO_CAPTURE:
      audio_allowed_ = allowed;
      break;
    case content::MEDIA_DEVICE_VIDEO_CAPTURE:
      video_allowed_ = allowed;
      break;
    default:
      NOTREACHED();
  }
}

void PublicSessionMediaAccessHandler::UserChoice::SetPrompted(
    content::MediaStreamType type) {
  switch (type) {
    case content::MEDIA_DEVICE_AUDIO_CAPTURE:
      audio_prompted_ = true;
      break;
    case content::MEDIA_DEVICE_VIDEO_CAPTURE:
      video_prompted_ = true;
      break;
    default:
      NOTREACHED();
  }
}
