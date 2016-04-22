// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "components/sessions/core/session_id.h"
#include "components/toolbar/test_toolbar_model.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace {

class TestingOmniboxView : public OmniboxView {
 public:
  explicit TestingOmniboxView(OmniboxEditController* controller)
      : OmniboxView(controller, nullptr) {}

  // OmniboxView:
  void Update() override {}
  void OpenMatch(const AutocompleteMatch& match,
                 WindowOpenDisposition disposition,
                 const GURL& alternate_nav_url,
                 const base::string16& pasted_text,
                 size_t selected_line) override {}
  base::string16 GetText() const override { return text_; }
  void SetUserText(const base::string16& text,
                   const base::string16& display_text,
                   bool update_popup) override {
    text_ = display_text;
  }
  void SetWindowTextAndCaretPos(const base::string16& text,
                                size_t caret_pos,
                                bool update_popup,
                                bool notify_text_changed) override {
    text_ = text;
  }
  void SetForcedQuery() override {}
  bool IsSelectAll() const override { return false; }
  bool DeleteAtEndPressed() override { return false; }
  void GetSelectionBounds(size_t* start, size_t* end) const override {}
  void SelectAll(bool reversed) override {}
  void RevertAll() override {}
  void UpdatePopup() override {}
  void SetFocus() override {}
  void ApplyCaretVisibility() override {}
  void OnTemporaryTextMaybeChanged(const base::string16& display_text,
                                   bool save_original_selection,
                                   bool notify_text_changed) override {
    text_ = display_text;
  }
  bool OnInlineAutocompleteTextMaybeChanged(const base::string16& display_text,
                                            size_t user_text_length) override {
    const bool text_changed = text_ != display_text;
    text_ = display_text;
    inline_autocomplete_text_ = display_text.substr(user_text_length);
    return text_changed;
  }
  void OnInlineAutocompleteTextCleared() override {
    inline_autocomplete_text_.clear();
  }
  void OnRevertTemporaryText() override {}
  void OnBeforePossibleChange() override {}
  bool OnAfterPossibleChange(bool allow_keyword_ui_change) override {
    return false;
  }
  gfx::NativeView GetNativeView() const override { return NULL; }
  gfx::NativeView GetRelativeWindowForPopup() const override { return NULL; }
  void SetGrayTextAutocompletion(const base::string16& input) override {}
  base::string16 GetGrayTextAutocompletion() const override {
    return base::string16();
  }
  int GetTextWidth() const override { return 0; }
  int GetWidth() const override { return 0; }
  bool IsImeComposing() const override { return false; }
  int GetOmniboxTextLength() const override { return 0; }
  void EmphasizeURLComponents() override {}

  const base::string16& inline_autocomplete_text() const {
    return inline_autocomplete_text_;
  }

 private:
  base::string16 text_;
  base::string16 inline_autocomplete_text_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxView);
};

class TestingOmniboxEditController : public OmniboxEditController {
 public:
  explicit TestingOmniboxEditController(ToolbarModel* toolbar_model)
      : toolbar_model_(toolbar_model) {}

 protected:
  // OmniboxEditController:
  void OnInputInProgress(bool in_progress) override {}
  void OnChanged() override {}
  void OnSetFocus() override {}
  void ShowURL() override {}
  ToolbarModel* GetToolbarModel() override { return toolbar_model_; }
  const ToolbarModel* GetToolbarModel() const override {
    return toolbar_model_;
  }

 private:
  ToolbarModel* toolbar_model_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxEditController);
};

class TestingSchemeClassifier : public AutocompleteSchemeClassifier {
 public:
  TestingSchemeClassifier() {}

  metrics::OmniboxInputType::Type GetInputTypeForScheme(
      const std::string& scheme) const override {
    return metrics::OmniboxInputType::URL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingSchemeClassifier);
};

class TestingOmniboxClient : public OmniboxClient {
 public:
  TestingOmniboxClient();
  ~TestingOmniboxClient() override;

  // OmniboxClient:
  std::unique_ptr<AutocompleteProviderClient> CreateAutocompleteProviderClient()
      override;

  std::unique_ptr<OmniboxNavigationObserver> CreateOmniboxNavigationObserver(
      const base::string16& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternate_nav_match) override {
    return nullptr;
  }
  bool CurrentPageExists() const override { return true; }
  const GURL& GetURL() const override { return GURL::EmptyGURL(); }
  const base::string16& GetTitle() const override {
    return base::EmptyString16();
  }
  gfx::Image GetFavicon() const override { return gfx::Image(); }
  bool IsInstantNTP() const override { return false; }
  bool IsSearchResultsPage() const override { return false; }
  bool IsLoading() const override { return false; }
  bool IsPasteAndGoEnabled() const override { return false; }
  bool IsNewTabPage(const std::string& url) const override { return false; }
  bool IsHomePage(const std::string& url) const override { return false; }
  const SessionID& GetSessionID() const override { return session_id_; }
  bookmarks::BookmarkModel* GetBookmarkModel() override { return nullptr; }
  TemplateURLService* GetTemplateURLService() override { return nullptr; }
  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }
  AutocompleteClassifier* GetAutocompleteClassifier() override {
    return &autocomplete_classifier_;
  }
  gfx::Image GetIconIfExtensionMatch(
      const AutocompleteMatch& match) const override {
    return gfx::Image();
  }
  bool ProcessExtensionKeyword(TemplateURL* template_url,
                               const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               OmniboxNavigationObserver* observer) override {
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
  void OnTextChanged(const AutocompleteMatch& current_match,
                     bool user_input_in_progress,
                     base::string16& user_text,
                     const AutocompleteResult& result,
                     bool is_popup_open,
                     bool has_focus) override {}
  void OnInputAccepted(const AutocompleteMatch& match) override {}
  void OnRevert() override {}
  void OnURLOpenedFromOmnibox(OmniboxLog* log) override {}
  void OnBookmarkLaunched() override {}
  void DiscardNonCommittedNavigations() override {}

 private:
  SessionID session_id_;
  TestingSchemeClassifier scheme_classifier_;
  AutocompleteClassifier autocomplete_classifier_;

  DISALLOW_COPY_AND_ASSIGN(TestingOmniboxClient);
};

TestingOmniboxClient::TestingOmniboxClient()
    : autocomplete_classifier_(
          base::WrapUnique(new AutocompleteController(
              CreateAutocompleteProviderClient(),
              nullptr,
              AutocompleteClassifier::kDefaultOmniboxProviders)),
          base::WrapUnique(new TestingSchemeClassifier())) {}

TestingOmniboxClient::~TestingOmniboxClient() {
  autocomplete_classifier_.Shutdown();
}

std::unique_ptr<AutocompleteProviderClient>
TestingOmniboxClient::CreateAutocompleteProviderClient() {
  std::unique_ptr<MockAutocompleteProviderClient> provider_client(
      new MockAutocompleteProviderClient());
  EXPECT_CALL(*provider_client.get(), GetBuiltinURLs())
      .WillRepeatedly(testing::Return(std::vector<base::string16>()));
  EXPECT_CALL(*provider_client.get(), GetSchemeClassifier())
      .WillRepeatedly(testing::ReturnRef(scheme_classifier_));

  std::unique_ptr<TemplateURLService> template_url_service(
      new TemplateURLService(
          nullptr, std::unique_ptr<SearchTermsData>(new SearchTermsData),
          nullptr, std::unique_ptr<TemplateURLServiceClient>(), nullptr,
          nullptr, base::Closure()));
  provider_client->set_template_url_service(std::move(template_url_service));

  return std::move(provider_client);
}

}  // namespace

class OmniboxEditTest : public ::testing::Test {
 public:
  TestToolbarModel* toolbar_model() { return &toolbar_model_; }

 private:
  base::MessageLoop message_loop_;
  TestToolbarModel toolbar_model_;
};

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST_F(OmniboxEditTest, AdjustTextForCopy) {
  struct Data {
    const char* perm_text;
    const int sel_start;
    const bool is_all_selected;
    const char* input;
    const char* expected_output;
    const bool write_url;
    const char* expected_url;
    const bool extracted_search_terms;
  } input[] = {
    // Test that http:// is inserted if all text is selected.
    { "a.de/b", 0, true, "a.de/b", "http://a.de/b", true, "http://a.de/b",
      false },

    // Test that http:// is inserted if the host is selected.
    { "a.de/b", 0, false, "a.de/", "http://a.de/", true, "http://a.de/",
      false },

    // Tests that http:// is inserted if the path is modified.
    { "a.de/b", 0, false, "a.de/c", "http://a.de/c", true, "http://a.de/c",
      false },

    // Tests that http:// isn't inserted if the host is modified.
    { "a.de/b", 0, false, "a.com/b", "a.com/b", false, "", false },

    // Tests that http:// isn't inserted if the start of the selection is 1.
    { "a.de/b", 1, false, "a.de/b", "a.de/b", false, "", false },

    // Tests that http:// isn't inserted if a portion of the host is selected.
    { "a.de/", 0, false, "a.d", "a.d", false, "", false },

    // Tests that http:// isn't inserted for an https url after the user nukes
    // https.
    { "https://a.com/", 0, false, "a.com/", "a.com/", false, "", false },

    // Tests that http:// isn't inserted if the user adds to the host.
    { "a.de/", 0, false, "a.de.com/", "a.de.com/", false, "", false },

    // Tests that we don't get double http if the user manually inserts http.
    { "a.de/", 0, false, "http://a.de/", "http://a.de/", true, "http://a.de/",
      false },

    // Makes sure intranet urls get 'http://' prefixed to them.
    { "b/foo", 0, true, "b/foo", "http://b/foo", true, "http://b/foo", false },

    // Verifies a search term 'foo' doesn't end up with http.
    { "www.google.com/search?", 0, false, "foo", "foo", false, "", false },

    // Makes sure extracted search terms are not modified.
    { "www.google.com/webhp?", 0, true, "hello world", "hello world", false,
      "", true },
  };
  TestingOmniboxEditController controller(toolbar_model());
  TestingOmniboxView view(&controller);
  OmniboxEditModel model(&view, &controller,
                         base::WrapUnique(new TestingOmniboxClient()));

  for (size_t i = 0; i < arraysize(input); ++i) {
    toolbar_model()->set_text(ASCIIToUTF16(input[i].perm_text));
    model.UpdatePermanentText();

    toolbar_model()->set_perform_search_term_replacement(
        input[i].extracted_search_terms);

    base::string16 result = ASCIIToUTF16(input[i].input);
    GURL url;
    bool write_url;
    model.AdjustTextForCopy(input[i].sel_start, input[i].is_all_selected,
                            &result, &url, &write_url);
    EXPECT_EQ(ASCIIToUTF16(input[i].expected_output), result) << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}

TEST_F(OmniboxEditTest, InlineAutocompleteText) {
  TestingOmniboxEditController controller(toolbar_model());
  TestingOmniboxView view(&controller);
  OmniboxEditModel model(&view, &controller,
                         base::WrapUnique(new TestingOmniboxClient()));

  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(base::string16(), view.inline_autocomplete_text());
  model.SetUserText(UTF8ToUTF16("he"));
  model.OnPopupDataChanged(UTF8ToUTF16("llo"), NULL, base::string16(), false);
  EXPECT_EQ(UTF8ToUTF16("hello"), view.GetText());
  EXPECT_EQ(UTF8ToUTF16("llo"), view.inline_autocomplete_text());

  model.OnAfterPossibleChange(UTF8ToUTF16("he"), UTF8ToUTF16("hel"), 3, 3,
                              false, true, false, true);
  EXPECT_EQ(base::string16(), view.inline_autocomplete_text());
  model.OnPopupDataChanged(UTF8ToUTF16("lo"), NULL, base::string16(), false);
  EXPECT_EQ(UTF8ToUTF16("hello"), view.GetText());
  EXPECT_EQ(UTF8ToUTF16("lo"), view.inline_autocomplete_text());

  model.Revert();
  EXPECT_EQ(base::string16(), view.GetText());
  EXPECT_EQ(base::string16(), view.inline_autocomplete_text());

  model.SetUserText(UTF8ToUTF16("he"));
  model.OnPopupDataChanged(UTF8ToUTF16("llo"), NULL, base::string16(), false);
  EXPECT_EQ(UTF8ToUTF16("hello"), view.GetText());
  EXPECT_EQ(UTF8ToUTF16("llo"), view.inline_autocomplete_text());

  model.AcceptTemporaryTextAsUserText();
  EXPECT_EQ(UTF8ToUTF16("hello"), view.GetText());
  EXPECT_EQ(base::string16(), view.inline_autocomplete_text());
}
