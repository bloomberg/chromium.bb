// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_search_prerenderer.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_unittest_base.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/render_messages.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_test_sink.h"
#include "ui/gfx/size.h"

namespace {

using content::Referrer;
using prerender::Origin;
using prerender::PrerenderContents;
using prerender::PrerenderHandle;
using prerender::PrerenderManager;
using prerender::PrerenderManagerFactory;

class DummyPrerenderContents : public PrerenderContents {
 public:
  DummyPrerenderContents(PrerenderManager* prerender_manager,
                         Profile* profile,
                         const GURL& url,
                         const Referrer& referrer,
                         Origin origin,
                         bool call_did_finish_load);

  virtual void StartPrerendering(
      int ALLOW_UNUSED creator_child_id,
      const gfx::Size& ALLOW_UNUSED size,
      content::SessionStorageNamespace* ALLOW_UNUSED session_storage_namespace)
      OVERRIDE;

 private:
  Profile* profile_;
  const GURL url_;
  bool call_did_finish_load_;

  DISALLOW_COPY_AND_ASSIGN(DummyPrerenderContents);
};

class DummyPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  explicit DummyPrerenderContentsFactory(bool call_did_finish_load)
      : call_did_finish_load_(call_did_finish_load) {
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      Profile* profile,
      const GURL& url,
      const Referrer& referrer,
      Origin origin,
      uint8 experiment_id) OVERRIDE;

 private:
  bool call_did_finish_load_;

  DISALLOW_COPY_AND_ASSIGN(DummyPrerenderContentsFactory);
};

DummyPrerenderContents::DummyPrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const Referrer& referrer,
    Origin origin,
    bool call_did_finish_load)
    : PrerenderContents(prerender_manager, profile, url, referrer, origin,
                        PrerenderManager::kNoExperiment),
      profile_(profile),
      url_(url),
      call_did_finish_load_(call_did_finish_load) {
}

void DummyPrerenderContents::StartPrerendering(
    int ALLOW_UNUSED creator_child_id,
    const gfx::Size& ALLOW_UNUSED size,
    content::SessionStorageNamespace* ALLOW_UNUSED session_storage_namespace) {
  prerender_contents_.reset(content::WebContents::Create(
      content::WebContents::CreateParams(profile_)));
  content::NavigationController::LoadURLParams params(url_);
  prerender_contents_->GetController().LoadURLWithParams(params);
  SearchTabHelper::CreateForWebContents(prerender_contents_.get());

  prerendering_has_started_ = true;
  NotifyPrerenderStart();

  if (call_did_finish_load_)
    DidFinishLoad(1, url_, true, NULL);
}

PrerenderContents* DummyPrerenderContentsFactory::CreatePrerenderContents(
    PrerenderManager* prerender_manager,
    Profile* profile,
    const GURL& url,
    const Referrer& referrer,
    Origin origin,
    uint8 experiment_id) {
  return new DummyPrerenderContents(prerender_manager, profile, url, referrer,
                                    origin, call_did_finish_load_);
}

}  // namespace

class InstantSearchPrerendererTest : public InstantUnitTestBase {
 public:
  InstantSearchPrerendererTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        "EmbeddedSearch",
        "Group1 strk:20 use_cacheable_ntp:1 prefetch_results:1"));
    InstantUnitTestBase::SetUp();
  }

  void Init(bool prerender_search_results_base_page,
            bool call_did_finish_load) {
    PrerenderManagerFactory::GetForProfile(browser()->profile())->
        SetPrerenderContentsFactory(
            new DummyPrerenderContentsFactory(call_did_finish_load));
    AddTab(browser(), GURL(content::kAboutBlankURL));

    if (prerender_search_results_base_page) {
      InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
      prerenderer->Init(
          GetActiveWebContents()->GetController()
              .GetSessionStorageNamespaceMap(),
          gfx::Size(640, 480));
      EXPECT_NE(static_cast<PrerenderHandle*>(NULL), prerender_handle());
    }
  }

  InstantSearchPrerenderer* GetInstantSearchPrerenderer() {
    return instant_service_->instant_search_prerenderer();
  }

  const GURL& GetPrerenderURL() {
    return GetInstantSearchPrerenderer()->prerender_url_;
  }

  void SetLastQuery(const string16& query) {
    GetInstantSearchPrerenderer()->last_instant_suggestion_ =
        InstantSuggestion(query, std::string());
  }

  content::WebContents* prerender_contents() {
    return GetInstantSearchPrerenderer()->prerender_contents();
  }

  bool MessageWasSent(uint32 id) {
    content::MockRenderProcessHost* process =
        static_cast<content::MockRenderProcessHost*>(
            prerender_contents()->GetRenderViewHost()->GetProcess());
    return process->sink().GetFirstMessageMatching(id) != NULL;
  }

  content::WebContents* GetActiveWebContents() const {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  PrerenderHandle* prerender_handle() {
    return GetInstantSearchPrerenderer()->prerender_handle_.get();
  }

};

// TODO(kmadhusu): Enable this after crrev.com/48113025 lands.
TEST_F(InstantSearchPrerendererTest,
       DISABLED_GetSearchTermsFromPrerenderedPage) {
  Init(false, false);
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  GURL url(GetPrerenderURL());
  EXPECT_EQ(GURL("https://www.google.com/instant?ion=1&foo=foo#foo=foo&strk"),
            url);
  EXPECT_EQ(UTF16ToASCII(prerenderer->get_last_query()),
            UTF16ToASCII(chrome::GetSearchTermsFromURL(profile(), url)));

  // Assume the prerendered page prefetched search results for the query
  // "flowers".
  SetLastQuery(ASCIIToUTF16("flowers"));
  EXPECT_EQ("flowers", UTF16ToASCII(prerenderer->get_last_query()));
  EXPECT_EQ(UTF16ToASCII(prerenderer->get_last_query()),
            UTF16ToASCII(chrome::GetSearchTermsFromURL(profile(), url)));
}

TEST_F(InstantSearchPrerendererTest, PrefetchSearchResults) {
  Init(true, true);
  EXPECT_TRUE(prerender_handle()->IsFinishedLoading());
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  prerenderer->Prerender(
      InstantSuggestion(ASCIIToUTF16("flowers"), std::string()));
  EXPECT_EQ("flowers", UTF16ToASCII(prerenderer->get_last_query()));
  EXPECT_TRUE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(InstantSearchPrerendererTest, DoNotPrefetchSearchResults) {
  Init(true, false);
  // Page hasn't finished loading yet.
  EXPECT_FALSE(prerender_handle()->IsFinishedLoading());
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  prerenderer->Prerender(
      InstantSuggestion(ASCIIToUTF16("flowers"), std::string()));
  EXPECT_EQ("", UTF16ToASCII(prerenderer->get_last_query()));
  EXPECT_FALSE(MessageWasSent(
      ChromeViewMsg_SearchBoxSetSuggestionToPrefetch::ID));
}

TEST_F(InstantSearchPrerendererTest, CanCommitQuery) {
  Init(true, true);
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  string16 query = ASCIIToUTF16("flowers");
  prerenderer->Prerender(InstantSuggestion(query, std::string()));
  EXPECT_TRUE(prerenderer->CanCommitQuery(GetActiveWebContents(), query));

  // Make sure InstantSearchPrerenderer::CanCommitQuery() returns false for
  // invalid search queries.
  EXPECT_FALSE(prerenderer->CanCommitQuery(GetActiveWebContents(),
                                           ASCIIToUTF16("joy")));
  EXPECT_FALSE(prerenderer->CanCommitQuery(GetActiveWebContents(), string16()));
}

TEST_F(InstantSearchPrerendererTest, CommitQuery) {
  Init(true, true);
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  string16 query = ASCIIToUTF16("flowers");
  prerenderer->Prerender(InstantSuggestion(query, std::string()));
  EXPECT_TRUE(prerenderer->CanCommitQuery(GetActiveWebContents(), query));
  prerenderer->Commit(query);
  EXPECT_TRUE(MessageWasSent(ChromeViewMsg_SearchBoxSubmit::ID));
}

// TODO(kmadhusu): Enable this after crrev.com/48113025 lands.
TEST_F(InstantSearchPrerendererTest,
       DISABLED_CancelPrerenderRequestOnTabChangeEvent) {
  Init(true, true);
  EXPECT_NE(static_cast<PrerenderHandle*>(NULL), prerender_handle());

  // Add a new tab to deactivate the current tab.
  AddTab(browser(), GURL(content::kAboutBlankURL));
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure the pending prerender request is cancelled.
  EXPECT_EQ(static_cast<PrerenderHandle*>(NULL), prerender_handle());
}

TEST_F(InstantSearchPrerendererTest, CancelPendingPrerenderRequest) {
  Init(true, true);
  EXPECT_NE(static_cast<PrerenderHandle*>(NULL), prerender_handle());

  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  prerenderer->Cancel();
  EXPECT_EQ(static_cast<PrerenderHandle*>(NULL), prerender_handle());
}

TEST_F(InstantSearchPrerendererTest, PrerenderingAllowed) {
  Init(true, true);
  InstantSearchPrerenderer* prerenderer = GetInstantSearchPrerenderer();
  content::WebContents* active_tab = GetActiveWebContents();
  EXPECT_EQ(GURL(content::kAboutBlankURL), active_tab->GetURL());

  // Allow prerendering only for search type AutocompleteMatch suggestions.
  AutocompleteMatch search_type_match(NULL, 1100, false,
                                      AutocompleteMatchType::SEARCH_SUGGEST);
  EXPECT_TRUE(AutocompleteMatch::IsSearchType(search_type_match.type));
  EXPECT_TRUE(prerenderer->IsAllowed(search_type_match, active_tab));

  AutocompleteMatch url_type_match(NULL, 1100, true,
                                   AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  EXPECT_FALSE(AutocompleteMatch::IsSearchType(url_type_match.type));
  EXPECT_FALSE(prerenderer->IsAllowed(url_type_match, active_tab));

  // Search results page supports Instant search. InstantSearchPrerenderer is
  // used only when the underlying page doesn't support Instant.
  NavigateAndCommitActiveTab(GURL("https://www.google.com/alt#quux=foo&strk"));
  active_tab = GetActiveWebContents();
  EXPECT_FALSE(chrome::GetSearchTermsFromURL(profile(), active_tab->GetURL())
      .empty());
  EXPECT_FALSE(chrome::ShouldPrefetchSearchResultsOnSRP());
  EXPECT_FALSE(prerenderer->IsAllowed(search_type_match, active_tab));
}
