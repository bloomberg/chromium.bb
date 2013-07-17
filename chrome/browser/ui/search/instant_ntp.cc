// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_ntp.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/webui/ntp/ntp_user_data_logger.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

InstantNTP::InstantNTP(InstantPage::Delegate* delegate,
                       const std::string& instant_url,
                       bool is_incognito)
    : InstantPage(delegate, instant_url, is_incognito),
      loader_(this) {
  DCHECK(delegate);
}

InstantNTP::~InstantNTP() {
}

void InstantNTP::InitContents(Profile* profile,
                              const content::WebContents* active_tab,
                              const base::Closure& on_stale_callback) {
  GURL instantNTP_url(instant_url());
  loader_.Init(instantNTP_url, profile, active_tab, on_stale_callback);
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

void InstantNTP::RenderViewCreated(content::RenderViewHost* render_view_host) {
  delegate()->InstantPageRenderViewCreated(contents());
}

void InstantNTP::RenderProcessGone(base::TerminationStatus /* status */) {
  delegate()->InstantPageRenderProcessGone(contents());
}

void InstantNTP::OnSwappedContents() {
  SetContents(loader_.contents());
}

void InstantNTP::OnFocus() {
  NOTREACHED();
}

void InstantNTP::OnMouseDown() {
  NOTREACHED();
}

void InstantNTP::OnMouseUp() {
  NOTREACHED();
}

content::WebContents* InstantNTP::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  return NULL;
}

void InstantNTP::LoadCompletedMainFrame() {
}
