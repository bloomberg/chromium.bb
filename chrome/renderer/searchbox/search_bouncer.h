// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "content/public/renderer/render_process_observer.h"
#include "url/gurl.h"

namespace IPC {
class Message;
}

// SearchBouncer tracks a set of URLs which should be transferred back to the
// browser process for potential reassignment to an Instant renderer process.
class SearchBouncer : public content::RenderProcessObserver {
 public:
  SearchBouncer();
  virtual ~SearchBouncer();

  // Returns whether a navigation to |url| should bounce back to the browser as
  // a potential Instant url. See chrome::ShouldAssignURLToInstantRenderer().
  bool ShouldFork(const GURL& url) const;

  // Returns whether |url| is a valid Instant new tab page URL.
  bool IsNewTabPage(const GURL& url) const;

  // Exposed for testing.
  void OnSetSearchURLs(std::vector<GURL> search_urls, GURL new_tab_page_url);

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchBouncerTest, SetSearchURLs);

  // From RenderProcessObserver:
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  // URLs to bounce back to the browser.
  std::vector<GURL> search_urls_;
  GURL new_tab_page_url_;

  DISALLOW_COPY_AND_ASSIGN(SearchBouncer);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
