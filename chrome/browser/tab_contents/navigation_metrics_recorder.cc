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
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "net/base/data_url.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

// List of mime types that can run JavaScript.
static const struct MimeTypePair {
  const char* const mime_type;
  NavigationMetricsRecorder::MimeType histogram_value;
} kScriptingMimeTypes[] = {
    {"text/html", NavigationMetricsRecorder::MIME_TYPE_HTML},
    {"application/xhtml+xml", NavigationMetricsRecorder::MIME_TYPE_XHTML},
    {"application/pdf", NavigationMetricsRecorder::MIME_TYPE_PDF},
    {"image/svg+xml", NavigationMetricsRecorder::MIME_TYPE_SVG}};

void RecordDataURLMimeType(const std::string& mime_type) {
  NavigationMetricsRecorder::MimeType value =
      NavigationMetricsRecorder::MIME_TYPE_OTHER;
  for (const MimeTypePair& pair : kScriptingMimeTypes) {
    if (mime_type == pair.mime_type) {
      value = pair.histogram_value;
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Navigation.MainFrameScheme.DataUrl.MimeType",
                            value, NavigationMetricsRecorder::MIME_TYPE_MAX);
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NavigationMetricsRecorder);

NavigationMetricsRecorder::NavigationMetricsRecorder(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      rappor_service_(g_browser_process->rappor_service()) {}

NavigationMetricsRecorder::~NavigationMetricsRecorder() {
}

void NavigationMetricsRecorder::set_rappor_service_for_testing(
    rappor::RapporServiceImpl* service) {
  rappor_service_ = service;
}

void NavigationMetricsRecorder::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!navigation_handle->IsInMainFrame() || !navigation_handle->HasCommitted())
    return;

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  navigation_metrics::OriginsSeenService* service =
      OriginsSeenServiceFactory::GetForBrowserContext(context);
  content::NavigationEntry* last_committed_entry =
      web_contents()->GetController().GetLastCommittedEntry();
  const url::Origin origin(last_committed_entry->GetVirtualURL());
  bool have_already_seen_origin = service->Insert(origin);

  navigation_metrics::RecordMainFrameNavigation(
      last_committed_entry->GetVirtualURL(), navigation_handle->IsSamePage(),
      context->IsOffTheRecord(), have_already_seen_origin);

  // Record the domain and registry of the URL that resulted in a navigation to
  // a |data:| URL, either by redirects or user clicking a link.
  if (last_committed_entry->GetVirtualURL().SchemeIs(url::kDataScheme) &&
      !ui::PageTransitionCoreTypeIs(navigation_handle->GetPageTransition(),
                                    ui::PAGE_TRANSITION_TYPED)) {
    if (!navigation_handle->GetPreviousURL().is_empty()) {
      rappor::SampleDomainAndRegistryFromGURL(
          rappor_service_, "Navigation.Scheme.Data",
          navigation_handle->GetPreviousURL());
    }

    // Also record the mime type of the data: URL.
    std::string mime_type;
    std::string charset;
    if (net::DataURL::Parse(last_committed_entry->GetVirtualURL(), &mime_type,
                            &charset, nullptr)) {
      RecordDataURLMimeType(mime_type);
    }
  }
}
