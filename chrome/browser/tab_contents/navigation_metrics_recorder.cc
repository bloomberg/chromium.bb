// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/metrics/histogram.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
#endif

DEFINE_WEB_CONTENTS_USER_DATA_KEY(NavigationMetricsRecorder);

namespace {

enum Scheme {
  SCHEME_UNKNOWN,
  SCHEME_HTTP,
  SCHEME_HTTPS,
  SCHEME_FILE,
  SCHEME_FTP,
  SCHEME_DATA,
  SCHEME_JAVASCRIPT,
  SCHEME_ABOUT,
  SCHEME_CHROME,
  SCHEME_MAX,
};

static const char* kSchemeNames[] = {
  "unknown",
  "http",
  "https",
  "file",
  "ftp",
  "data",
  "javascript",
  "about",
  "chrome",
  "max",
};

COMPILE_ASSERT(arraysize(kSchemeNames) == SCHEME_MAX + 1,
               NavigationMetricsRecorder_name_count_mismatch);

void RecordMainFrameNavigation(const content::LoadCommittedDetails& details) {
  GURL url = details.entry->GetVirtualURL();
  Scheme scheme = SCHEME_UNKNOWN;
  for (int i = 1; i < SCHEME_MAX; ++i) {
    if (url.SchemeIs(kSchemeNames[i])) {
      scheme = static_cast<Scheme>(i);
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION(
      "Navigation.MainFrameScheme", scheme, SCHEME_MAX);
}

}  // namespace

NavigationMetricsRecorder::NavigationMetricsRecorder(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

NavigationMetricsRecorder::~NavigationMetricsRecorder() {
}

void NavigationMetricsRecorder::DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) {
  RecordMainFrameNavigation(details);
}

void NavigationMetricsRecorder::DidStartLoading(
    content::RenderViewHost* render_view_host) {
#if defined(OS_WIN) && defined(USE_ASH)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
    gfx::NativeView view = render_view_host->GetView()->GetNativeView();
    if (view) {
      chrome::HostDesktopType desktop =
          chrome::GetHostDesktopTypeForNativeView(view);
      UMA_HISTOGRAM_ENUMERATION("Win8.PageLoad",
                                chrome::GetWin8Environment(desktop),
                                chrome::WIN_8_ENVIRONMENT_MAX);
    }
  }
#endif
}


