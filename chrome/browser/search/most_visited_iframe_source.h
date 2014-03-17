// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_
#define CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/search/iframe_source.h"

// Serves HTML for displaying suggestions using iframes, e.g.
// chrome-search://suggestion/loader.html
class MostVisitedIframeSource : public IframeSource {
 public:
  MostVisitedIframeSource();
  virtual ~MostVisitedIframeSource();

  // Number of Most Visited elements on the NTP for logging purposes.
  static const int kNumMostVisited;
  // Name of the histogram keeping track of Most Visited clicks.
  static const char kMostVisitedHistogramName[];

  // Overridden from IframeSource. Public for testing.
  virtual void StartDataRequest(
      const std::string& path_and_query,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;

 protected:
  // Overridden from IframeSource:
  virtual std::string GetSource() const OVERRIDE;

  virtual bool ServesPath(const std::string& path) const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(MostVisitedIframeSourceTest,
                           LogEndpointIsValidWithProvider);

  DISALLOW_COPY_AND_ASSIGN(MostVisitedIframeSource);
};

#endif  // CHROME_BROWSER_SEARCH_MOST_VISITED_IFRAME_SOURCE_H_
