// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_tab_helper_base.h"

#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/components/web_app_audio_focus_id_map.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_handle.h"

namespace web_app {

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebAppTabHelperBase)

WebAppTabHelperBase::WebAppTabHelperBase(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

WebAppTabHelperBase::~WebAppTabHelperBase() = default;

void WebAppTabHelperBase::SetAudioFocusIdMap(
    WebAppAudioFocusIdMap* audio_focus_id_map) {
  DCHECK(!audio_focus_id_map_ && audio_focus_id_map);

  audio_focus_id_map_ = audio_focus_id_map;
}

void WebAppTabHelperBase::SetAppId(const AppId& app_id) {
  if (app_id_ == app_id)
    return;

  app_id_ = app_id;

  OnAssociatedAppChanged();
}

void WebAppTabHelperBase::ResetAppId() {
  app_id_.clear();

  OnAssociatedAppChanged();
}

void WebAppTabHelperBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  const AppId app_id = GetAppId(navigation_handle->GetURL());
  SetAppId(app_id);
}

void WebAppTabHelperBase::DidCloneToNewWebContents(
    content::WebContents* old_web_contents,
    content::WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a WebAppTabHelperBase.
  WebAppTabHelperBase* new_tab_helper = CloneForWebContents(new_web_contents);
  // Clone common state:
  new_tab_helper->SetAppId(app_id());
}

void WebAppTabHelperBase::OnAssociatedAppChanged() {
  UpdateAudioFocusGroupId();
}

void WebAppTabHelperBase::UpdateAudioFocusGroupId() {
  DCHECK(audio_focus_id_map_);

  if (!app_id_.empty() && IsInAppWindow()) {
    audio_focus_group_id_ = audio_focus_id_map_->CreateOrGetIdForApp(app_id_);
  } else {
    audio_focus_group_id_ = base::UnguessableToken::Null();
  }

  content::MediaSession::Get(web_contents())
      ->SetAudioFocusGroupId(audio_focus_group_id_);
}

}  // namespace web_app
