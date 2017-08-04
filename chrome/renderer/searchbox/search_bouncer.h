// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
#define CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/common/search.mojom.h"
#include "content/public/common/associated_interface_registry.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "url/gurl.h"

// SearchBouncer tracks a set of URLs which should be transferred back to the
// browser process for potential reassignment to an Instant renderer process.
class SearchBouncer : public content::RenderThreadObserver,
                      public chrome::mojom::SearchBouncer {
 public:
  SearchBouncer();
  ~SearchBouncer() override;

  static SearchBouncer* GetInstance();

  // RenderThreadObserver:
  void RegisterMojoInterfaces(
      content::AssociatedInterfaceRegistry* associated_interfaces) override;

  // Returns whether a navigation to |url| should bounce back to the browser as
  // a potential Instant url. See search::ShouldAssignURLToInstantRenderer().
  bool ShouldFork(const GURL& url) const;

  // Returns whether |url| is a valid Instant new tab page URL.
  bool IsNewTabPage(const GURL& url) const;

  // chrome::mojom::SearchBouncer:
  void SetSearchURLs(const std::vector<GURL>& search_urls,
                     const GURL& new_tab_page_url) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchBouncerTest, SetSearchURLs);

  void OnSearchBouncerRequest(
      chrome::mojom::SearchBouncerAssociatedRequest request);

  // URLs to bounce back to the browser.
  std::vector<GURL> search_urls_;
  GURL new_tab_page_url_;

  mojo::AssociatedBinding<chrome::mojom::SearchBouncer> search_bouncer_binding_;

  DISALLOW_COPY_AND_ASSIGN(SearchBouncer);
};

#endif  // CHROME_RENDERER_SEARCHBOX_SEARCH_BOUNCER_H_
