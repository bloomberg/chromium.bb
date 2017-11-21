// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/omnibox_edit_model.h"

#include <stddef.h>
#include <memory>

#include "base/memory/ptr_util.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/test_omnibox_client.h"
#include "components/omnibox/browser/test_omnibox_edit_controller.h"
#include "components/omnibox/browser/test_omnibox_view.h"
#include "testing/gtest/include/gtest/gtest.h"

class OmniboxEditModelTest : public testing::Test {
 public:
  void SetUp() override {
    controller_ = base::MakeUnique<TestOmniboxEditController>();
    view_ = base::MakeUnique<TestOmniboxView>(controller_.get());
    model_ = base::MakeUnique<OmniboxEditModel>(
        view_.get(), controller_.get(), base::MakeUnique<TestOmniboxClient>());
  }

  const TestOmniboxView& view() { return *view_; }
  OmniboxEditModel* model() { return model_.get(); }

 private:
  std::unique_ptr<TestOmniboxEditController> controller_;
  std::unique_ptr<TestOmniboxView> view_;
  std::unique_ptr<OmniboxEditModel> model_;
};

// Tests various permutations of AutocompleteModel::AdjustTextForCopy.
TEST_F(OmniboxEditModelTest, AdjustTextForCopy) {
  struct Data {
    const char* perm_text;
    const int sel_start;
    const bool is_all_selected;
    const char* input;
    const char* expected_output;
    const bool write_url;
    const char* expected_url;
  } input[] = {
      // Test that http:// is inserted if all text is selected.
      {"a.de/b", 0, true, "a.de/b", "http://a.de/b", true, "http://a.de/b"},

      // Test that http:// is inserted if the host is selected.
      {"a.de/b", 0, false, "a.de/", "http://a.de/", true, "http://a.de/"},

      // Tests that http:// is inserted if the path is modified.
      {"a.de/b", 0, false, "a.de/c", "http://a.de/c", true, "http://a.de/c"},

      // Tests that http:// isn't inserted if the host is modified.
      {"a.de/b", 0, false, "a.com/b", "a.com/b", false, ""},

      // Tests that http:// isn't inserted if the start of the selection is 1.
      {"a.de/b", 1, false, "a.de/b", "a.de/b", false, ""},

      // Tests that http:// isn't inserted if a portion of the host is selected.
      {"a.de/", 0, false, "a.d", "a.d", false, ""},

      // Tests that http:// isn't inserted for an https url after the user nukes
      // https.
      {"https://a.com/", 0, false, "a.com/", "a.com/", false, ""},

      // Tests that http:// isn't inserted if the user adds to the host.
      {"a.de/", 0, false, "a.de.com/", "a.de.com/", false, ""},

      // Tests that we don't get double http if the user manually inserts http.
      {"a.de/", 0, false, "http://a.de/", "http://a.de/", true, "http://a.de/"},

      // Makes sure intranet urls get 'http://' prefixed to them.
      {"b/foo", 0, true, "b/foo", "http://b/foo", true, "http://b/foo"},

      // Verifies a search term 'foo' doesn't end up with http.
      {"www.google.com/search?", 0, false, "foo", "foo", false, ""},
  };

  for (size_t i = 0; i < arraysize(input); ++i) {
    model()->SetPermanentText(base::ASCIIToUTF16(input[i].perm_text));

    base::string16 result = base::ASCIIToUTF16(input[i].input);
    GURL url;
    bool write_url;
    model()->AdjustTextForCopy(input[i].sel_start, input[i].is_all_selected,
                               &result, &url, &write_url);
    EXPECT_EQ(base::ASCIIToUTF16(input[i].expected_output), result)
        << "@: " << i;
    EXPECT_EQ(input[i].write_url, write_url) << " @" << i;
    if (write_url)
      EXPECT_EQ(input[i].expected_url, url.spec()) << " @" << i;
  }
}

TEST_F(OmniboxEditModelTest, InlineAutocompleteText) {
  // Test if the model updates the inline autocomplete text in the view.
  EXPECT_EQ(base::string16(), view().inline_autocomplete_text());
  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::ASCIIToUTF16("llo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view().GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view().inline_autocomplete_text());

  base::string16 text_before = base::ASCIIToUTF16("he");
  base::string16 text_after = base::ASCIIToUTF16("hel");
  OmniboxView::StateChanges state_changes{
      &text_before, &text_after, 3, 3, false, true, false, false};
  model()->OnAfterPossibleChange(state_changes, true);
  EXPECT_EQ(base::string16(), view().inline_autocomplete_text());
  model()->OnPopupDataChanged(base::ASCIIToUTF16("lo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view().GetText());
  EXPECT_EQ(base::ASCIIToUTF16("lo"), view().inline_autocomplete_text());

  model()->Revert();
  EXPECT_EQ(base::string16(), view().GetText());
  EXPECT_EQ(base::string16(), view().inline_autocomplete_text());

  model()->SetUserText(base::ASCIIToUTF16("he"));
  model()->OnPopupDataChanged(base::ASCIIToUTF16("llo"), nullptr,
                              base::string16(), false);
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view().GetText());
  EXPECT_EQ(base::ASCIIToUTF16("llo"), view().inline_autocomplete_text());

  model()->AcceptTemporaryTextAsUserText();
  EXPECT_EQ(base::ASCIIToUTF16("hello"), view().GetText());
  EXPECT_EQ(base::string16(), view().inline_autocomplete_text());
}

// This verifies the fix for a bug where calling OpenMatch() with a valid
// alternate nav URL would fail a DCHECK if the input began with "http://".
// The failure was due to erroneously trying to strip the scheme from the
// resulting fill_into_edit.  Alternate nav matches are never shown, so there's
// no need to ever try and strip this scheme.
TEST_F(OmniboxEditModelTest, AlternateNavHasHTTP) {
  const TestOmniboxClient* client =
      static_cast<TestOmniboxClient*>(model()->client());
  const AutocompleteMatch match(
      model()->autocomplete_controller()->search_provider(), 0, false,
      AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED);
  const GURL alternate_nav_url("http://abcd/");

  model()->OnSetFocus(false);  // Avoids DCHECK in OpenMatch().
  model()->SetUserText(base::ASCIIToUTF16("http://abcd"));
  model()->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB,
                     alternate_nav_url, base::string16(), 0);
  EXPECT_TRUE(AutocompleteInput::HasHTTPScheme(
      client->alternate_nav_match().fill_into_edit));

  model()->SetUserText(base::ASCIIToUTF16("abcd"));
  model()->OpenMatch(match, WindowOpenDisposition::CURRENT_TAB,
                     alternate_nav_url, base::string16(), 0);
  EXPECT_TRUE(AutocompleteInput::HasHTTPScheme(
      client->alternate_nav_match().fill_into_edit));
}
