// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_ntp.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "googleurl/src/gurl.h"

namespace {

// Helper class for logging data from the NTP. Attached to each InstantNTP
// instance.
class NTPLoggingUserData
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NTPLoggingUserData> {
 public:
  virtual ~NTPLoggingUserData() {}

  // Called each time the mouse hovers over an iframe or title.
  void increment_number_of_mouseovers() {
    number_of_mouseovers_++;
  }

  void set_instant_url(const GURL& url) {
    instant_url_ = url;
  }

  // Logs total number of mouseovers per NTP session to UMA histogram. Called
  // when an NTP tab is about to be deactivated (be it by switching tabs, losing
  // focus or closing the tab/shutting down Chrome) or when the user navigates
  // to a URL.
  void EmitMouseoverCount() {
    UMA_HISTOGRAM_COUNTS("NewTabPage.NumberOfMouseOvers",
                         number_of_mouseovers_);
    number_of_mouseovers_ = 0;
  }

  // content::WebContentsObserver override
  virtual void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) OVERRIDE {
    if (!load_details.previous_url.is_valid())
      return;

    if (chrome::MatchesOriginAndPath(instant_url_, load_details.previous_url))
      EmitMouseoverCount();
  }

 private:
  explicit NTPLoggingUserData(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        number_of_mouseovers_(0) {}
  friend class content::WebContentsUserData<NTPLoggingUserData>;

  int number_of_mouseovers_;
  GURL instant_url_;

  DISALLOW_COPY_AND_ASSIGN(NTPLoggingUserData);
};

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NTPLoggingUserData);

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

  NTPLoggingUserData::CreateForWebContents(content);
  NTPLoggingUserData::FromWebContents(content)->set_instant_url(instantNTP_url);

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

// static
void InstantNTP::CountMouseover(content::WebContents* contents) {
  NTPLoggingUserData* data = NTPLoggingUserData::FromWebContents(contents);
  if (data)
    data->increment_number_of_mouseovers();
}

// static
void InstantNTP::EmitMouseoverCount(content::WebContents* contents) {
  NTPLoggingUserData* data = NTPLoggingUserData::FromWebContents(contents);
  if (data)
    data->EmitMouseoverCount();
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
