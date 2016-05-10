// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/tab_contents/origins_seen_service_factory.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "components/navigation_metrics/origins_seen_service.h"
#include "components/rappor/rappor_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NavigationMetricsRecorder);

NavigationMetricsRecorder::NavigationMetricsRecorder(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

NavigationMetricsRecorder::~NavigationMetricsRecorder() {
}

void NavigationMetricsRecorder::DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  navigation_metrics::OriginsSeenService* service =
      OriginsSeenServiceFactory::GetForBrowserContext(context);
  const url::Origin origin(details.entry->GetVirtualURL());
  bool have_already_seen_origin = service->Insert(origin);

  navigation_metrics::RecordMainFrameNavigation(
      details.entry->GetVirtualURL(), details.is_in_page,
      context->IsOffTheRecord(), have_already_seen_origin);

  // Record the domain and registry of the URL that resulted in a navigation to
  // a |data:| URL, either by redirects or user clicking a link.
  if (details.entry->GetVirtualURL().SchemeIs(url::kDataScheme) &&
      params.transition != ui::PAGE_TRANSITION_TYPED &&
      !details.previous_url.is_empty()) {
    rappor::SampleDomainAndRegistryFromGURL(g_browser_process->rappor_service(),
                                            "Navigation.Scheme.Data",
                                            details.previous_url);
  }
}
