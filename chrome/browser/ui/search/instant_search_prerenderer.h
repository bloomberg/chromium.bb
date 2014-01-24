// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_INSTANT_SEARCH_PRERENDERER_H_
#define CHROME_BROWSER_UI_SEARCH_INSTANT_SEARCH_PRERENDERER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/common/instant_types.h"
#include "content/public/browser/navigation_controller.h"

class GURL;
class Profile;
struct AutocompleteMatch;

namespace chrome {
struct NavigateParams;
}

namespace content {
class WebContents;
}

namespace gfx {
class Size;
}

namespace prerender {
class PrerenderHandle;
}

// InstantSearchPrerenderer is responsible for prerendering the default search
// provider Instant search base page URL to prefetch high-confidence search
// suggestions. InstantSearchPrerenderer manages the prerender handle associated
// with the prerendered contents.
class InstantSearchPrerenderer {
 public:
  InstantSearchPrerenderer(Profile* profile, const GURL& url);
  ~InstantSearchPrerenderer();

  // Returns the InstantSearchPrerenderer instance for the given |profile|.
  static InstantSearchPrerenderer* GetForProfile(Profile* profile);

  // Prerender the |prerender_url_| contents. The
  // |session_storage_namespace_map| is used to identify the namespace of the
  // active tab at the time the prerender is generated. The |size| gives the
  // initial size for the target prerender. InstantSearchPrerenderer will run at
  // most one prerender at a time, so launching a prerender will cancel the
  // previous prerenders (if any).
  void Init(
      const content::SessionStorageNamespaceMap& session_storage_namespace_map,
      const gfx::Size& size);

  // Cancels the current request.
  void Cancel();

  // Tells the Instant search base page to prerender |suggestion|.
  void Prerender(const InstantSuggestion& suggestion);

  // Tells the Instant search base page to render the search results for the
  // given |query|.
  void Commit(const base::string16& query);

  // Returns true if the prerendered page can be used to process the search for
  // the given |source|.
  bool CanCommitQuery(content::WebContents* source,
                      const base::string16& query) const;

  // Returns true and updates |params->target_contents| if a prerendered page
  // exists for |url| and is swapped in.
  bool UsePrerenderedPage(const GURL& url, chrome::NavigateParams* params);

  // Returns the last prefetched search query.
  const base::string16& get_last_query() const {
    return last_instant_suggestion_.text;
  }

  // Returns true when prerendering is allowed for the given |source| and
  // |match|.
  bool IsAllowed(const AutocompleteMatch& match,
                 content::WebContents* source) const;

 private:
  friend class InstantSearchPrerendererTest;

  content::WebContents* prerender_contents() const;

  // Returns true if the |query| matches the last prefetched search query or if
  // the 'reuse_instant_search_base_page' flag is enabled in the field trials.
  bool QueryMatchesPrefetch(const base::string16& query) const;

  Profile* const profile_;

  // Instant search base page URL.
  const GURL prerender_url_;

  scoped_ptr<prerender::PrerenderHandle> prerender_handle_;

  InstantSuggestion last_instant_suggestion_;

  DISALLOW_COPY_AND_ASSIGN(InstantSearchPrerenderer);
};

#endif  // CHROME_BROWSER_UI_SEARCH_INSTANT_SEARCH_PRERENDERER_H_
