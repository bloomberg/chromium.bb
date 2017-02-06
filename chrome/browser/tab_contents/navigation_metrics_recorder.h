// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_
#define CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace rappor {
class RapporServiceImpl;
}

class NavigationMetricsRecorder
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationMetricsRecorder> {
 public:
  ~NavigationMetricsRecorder() override;

  void set_rappor_service_for_testing(
      rappor::RapporServiceImpl* rappor_service);

  // This enum is used in building a histogram. So, this is append only,
  // any new scheme should be added at the end, before MIME_TYPE_MAX.
  enum MimeType {
    MIME_TYPE_OTHER,
    MIME_TYPE_HTML,
    MIME_TYPE_XHTML,
    MIME_TYPE_PDF,
    MIME_TYPE_SVG,
    MIME_TYPE_MAX
  };

 private:
  explicit NavigationMetricsRecorder(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NavigationMetricsRecorder>;

  // content::WebContentsObserver overrides:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  rappor::RapporServiceImpl* rappor_service_;

  DISALLOW_COPY_AND_ASSIGN(NavigationMetricsRecorder);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_NAVIGATION_METRICS_RECORDER_H_
