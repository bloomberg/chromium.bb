// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_engagement/desktop_engagement_observer.h"

#include "chrome/browser/metrics/desktop_engagement/desktop_engagement_service.h"
#include "content/public/browser/render_view_host.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(metrics::DesktopEngagementObserver);

namespace metrics {

DesktopEngagementObserver::DesktopEngagementObserver(
    content::WebContents* web_contents,
    DesktopEngagementService* service)
    : content::WebContentsObserver(web_contents), service_(service) {
  RegisterInputEventObserver(web_contents->GetRenderViewHost());
}

DesktopEngagementObserver::~DesktopEngagementObserver() {}

// static
DesktopEngagementObserver* DesktopEngagementObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (!DesktopEngagementService::IsInitialized())
    return nullptr;

  DesktopEngagementObserver* observer = FromWebContents(web_contents);
  if (!observer) {
    observer = new DesktopEngagementObserver(web_contents,
                                             DesktopEngagementService::Get());
    web_contents->SetUserData(UserDataKey(), observer);
  }
  return observer;
}

void DesktopEngagementObserver::RegisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->AddInputEventObserver(this);
}

void DesktopEngagementObserver::UnregisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->RemoveInputEventObserver(this);
}

void DesktopEngagementObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  service_->OnUserEvent();
}

void DesktopEngagementObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

}  // namespace metrics
