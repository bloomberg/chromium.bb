// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"

#include "base/metrics/histogram.h"
#include "components/navigation_metrics/navigation_metrics.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/metro_utils/metro_chrome_win.h"
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
  navigation_metrics::RecordMainFrameNavigation(details.entry->GetVirtualURL());
}

void NavigationMetricsRecorder::DidStartLoading(
    content::RenderViewHost* render_view_host) {
#if defined(OS_WIN) && defined(USE_ASH)
  if (render_view_host && base::win::GetVersion() >= base::win::VERSION_WIN8) {
    content::RenderWidgetHostView* rwhv = render_view_host->GetView();
    if (rwhv) {
      gfx::NativeView native_view = rwhv->GetNativeView();
      if (native_view) {
        chrome::HostDesktopType desktop =
            chrome::GetHostDesktopTypeForNativeView(native_view);
        UMA_HISTOGRAM_ENUMERATION("Win8.PageLoad",
                                  chrome::GetWin8Environment(desktop),
                                  chrome::WIN_8_ENVIRONMENT_MAX);
      }
    }
  }
#endif
}


