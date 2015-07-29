// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/ui/omnibox/omnibox_client.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/sessions/session_id.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class TestOmniboxClient : public OmniboxClient {
 public:
  TestOmniboxClient(content::WebContents* web_contents, Profile* profile)
      : web_contents_(web_contents), profile_(profile) {}
  ~TestOmniboxClient() override {}

  // OmniboxClient:
  scoped_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override {
    return make_scoped_ptr(new ChromeAutocompleteProviderClient(profile_));
  }
  bool CurrentPageExists() const override { return true; }
  const GURL& GetURL() const override { return web_contents_->GetURL(); }
  bool IsInstantNTP() const override { return false; }
  bool IsSearchResultsPage() const override { return false; }
  bool IsLoading() const override { return false; }
  bool IsPasteAndGoEnabled() const override { return false; }
  content::NavigationController& GetNavigationController() const override {
    return web_contents_->GetController();
  }
  const SessionID& GetSessionID() const override { return session_id_; }
  bool ProcessExtensionKeyword(TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition) override {
    return false;
  }
  void OnInputStateChanged() override {}
  void OnFocusChanged(OmniboxFocusState state,
                      OmniboxFocusChangeReason reason) override {}
  void OnResultChanged(const AutocompleteResult& result,
                       bool default_match_changed,
                       const base::Callback<void(const SkBitmap& bitmap)>&
                           on_bitmap_fetched) override {}
  void OnCurrentMatchChanged(const AutocompleteMatch& match) override {}
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override {}
  void DoPrerender(const AutocompleteMatch& match) override {}
  void DoPreconnect(const AutocompleteMatch& match) override {}

 private:
  content::WebContents* web_contents_;
  Profile* profile_;
  SessionID session_id_;

  DISALLOW_COPY_AND_ASSIGN(TestOmniboxClient);
};

}  // namespace

class OmniboxControllerTest : public ChromeRenderViewHostTestHarness {
 protected:
  OmniboxControllerTest();
  ~OmniboxControllerTest() override;

  void CreateController();
  void AssertProviders(int expected_providers);

  PrefService* GetPrefs() { return profile()->GetPrefs(); }
  const AutocompleteController::Providers& GetAutocompleteProviders() const {
    return omnibox_controller_->autocomplete_controller()->providers();
  }

 private:
  // ChromeRenderViewTestHarness:
  void SetUp() override;
  void TearDown() override;

  scoped_ptr<TestOmniboxClient> omnibox_client_;
  scoped_ptr<OmniboxController> omnibox_controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxControllerTest);
};

OmniboxControllerTest::OmniboxControllerTest() {
}

OmniboxControllerTest::~OmniboxControllerTest() {
}

void OmniboxControllerTest::CreateController() {
  DCHECK(omnibox_client_);
  omnibox_controller_.reset(new OmniboxController(NULL, omnibox_client_.get()));
}

// Checks that the list of autocomplete providers used by the OmniboxController
// matches the one in the |expected_providers| bit field.
void OmniboxControllerTest::AssertProviders(int expected_providers) {
  const AutocompleteController::Providers& providers =
      GetAutocompleteProviders();

  for (size_t i = 0; i < providers.size(); ++i) {
    // Ensure this is a provider we wanted.
    int type = providers[i]->type();
    ASSERT_TRUE(expected_providers & type);

    // Remove it from expectations so we fail if it's there twice.
    expected_providers &= ~type;
  }

  // Ensure we saw all the providers we expected.
  ASSERT_EQ(0, expected_providers);
}

void OmniboxControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  omnibox_client_.reset(new TestOmniboxClient(web_contents(), profile()));
}

void OmniboxControllerTest::TearDown() {
  omnibox_controller_.reset();
  omnibox_client_.reset();

  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(OmniboxControllerTest, CheckDefaultAutocompleteProviders) {
  CreateController();
  // First collect the basic providers.
  int observed_providers = 0;
  const AutocompleteController::Providers& providers =
      GetAutocompleteProviders();
  for (size_t i = 0; i < providers.size(); ++i)
    observed_providers |= providers[i]->type();
  // Ensure we have at least one provider.
  ASSERT_NE(0, observed_providers);

  // Ensure instant extended includes all the provides in classic Chrome.
  int providers_with_instant_extended = observed_providers;
  // TODO(beaudoin): remove TYPE_SEARCH once it's no longer needed to pass
  // the Instant suggestion through via FinalizeInstantQuery.
  CreateController();
  AssertProviders(providers_with_instant_extended);
}
