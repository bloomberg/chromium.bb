// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_ntp.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/search/instant_ntp_prerenderer.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

InstantNTP::InstantNTP(InstantNTPPrerenderer* ntp_prerenderer,
                       const std::string& instant_url,
                       Profile* profile)
    : InstantPage(ntp_prerenderer, instant_url, profile,
                  profile->IsOffTheRecord()),
      loader_(this),
      ntp_prerenderer_(ntp_prerenderer) {
}

InstantNTP::~InstantNTP() {
  if (contents())
    ReleaseContents().reset();
}

void InstantNTP::InitContents(const base::Closure& on_stale_callback) {
  DCHECK(!contents());
  GURL instantNTP_url(instant_url());
  loader_.Init(instantNTP_url, profile(), on_stale_callback);
  SetContents(loader_.contents());
  content::WebContents* content = contents();
  SearchTabHelper::FromWebContents(content)->InitForPreloadedNTP();

  NTPUserDataLogger::CreateForWebContents(content);
  NTPUserDataLogger::FromWebContents(content)->set_ntp_url(instantNTP_url);

  loader_.Load();
}

scoped_ptr<content::WebContents> InstantNTP::ReleaseContents() {
  SetContents(NULL);
  return loader_.ReleaseContents();
}

void InstantNTP::LoadCompletedMainFrame() {
  ntp_prerenderer_->LoadCompletedMainFrame();
}

void InstantNTP::RenderProcessGone(base::TerminationStatus /* status */) {
  ntp_prerenderer_->RenderProcessGone();
}

void InstantNTP::OnSwappedContents() {
  SetContents(loader_.contents());
}

content::WebContents* InstantNTP::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return NULL;
}
