// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_engagement_service.h"

#include "chrome/browser/media/media_engagement_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/base/media_switches.h"

class MediaEngagementService::ContentsObserver
    : public content::WebContentsObserver {
 public:
  ContentsObserver(content::WebContents* web_contents,
                   MediaEngagementService* service)
      : WebContentsObserver(web_contents), service_(service) {}

  ~ContentsObserver() override = default;

  // WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    service_->contents_observers_.erase(this);
    delete this;
  }

 private:
  // |this| is owned by |service_|.
  MediaEngagementService* service_;

  DISALLOW_COPY_AND_ASSIGN(ContentsObserver);
};

// static
bool MediaEngagementService::IsEnabled() {
  return base::FeatureList::IsEnabled(media::kMediaEngagement);
}

// static
MediaEngagementService* MediaEngagementService::Get(Profile* profile) {
  DCHECK(IsEnabled());
  return MediaEngagementServiceFactory::GetForProfile(profile);
}

// static
void MediaEngagementService::CreateWebContentsObserver(
    content::WebContents* web_contents) {
  DCHECK(IsEnabled());
  MediaEngagementService* service =
      Get(Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!service)
    return;
  service->contents_observers_.insert(
      new ContentsObserver(web_contents, service));
}

MediaEngagementService::MediaEngagementService(Profile* profile) {
  DCHECK(IsEnabled());
}

MediaEngagementService::~MediaEngagementService() = default;
