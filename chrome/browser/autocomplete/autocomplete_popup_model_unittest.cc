// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_popup_model.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoCompletePopupModelTest : public testing::Test {
 public:
  AutoCompletePopupModelTest() {
  }

  virtual void SetUp();
  virtual void TearDown();

 protected:
  AutocompleteMatch CreateMatch(const string16& keyword,
                                const string16& query_string,
                                AutocompleteMatch::Type type);
  void RunTest(const char* input, AutocompleteMatch::Type type,
               const char* keyword);

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<AutocompletePopupModel> model_;
  scoped_ptr<AutocompleteEditModel> edit_model_;
};

void AutoCompletePopupModelTest::SetUp() {
  profile_.reset(new TestingProfile());
  profile_->CreateTemplateURLService();
  edit_model_.reset(new AutocompleteEditModel(NULL, NULL, profile_.get()));
  model_.reset(new AutocompletePopupModel(NULL, edit_model_.get()));

  TemplateURLService* turl_model =
      TemplateURLServiceFactory::GetForProfile(profile_.get());

  turl_model->Load();

  // Reset the default TemplateURL.
  TemplateURL* default_t_url = new TemplateURL();
  default_t_url->set_keyword(ASCIIToUTF16("t"));
  default_t_url->SetURL("http://defaultturl/{searchTerms}", 0, 0);
  turl_model->Add(default_t_url);
  turl_model->SetDefaultSearchProvider(default_t_url);
  ASSERT_NE(0, default_t_url->id());

  // Create another TemplateURL for KeywordProvider.
  TemplateURL* keyword_t_url = new TemplateURL();
  keyword_t_url->set_short_name(ASCIIToUTF16("k"));
  keyword_t_url->set_keyword(ASCIIToUTF16("k"));
  keyword_t_url->SetURL("http://keyword/{searchTerms}", 0, 0);
  turl_model->Add(keyword_t_url);
  ASSERT_NE(0, keyword_t_url->id());
}

void AutoCompletePopupModelTest::TearDown() {
  profile_.reset();
  model_.reset();
  edit_model_.reset();
}

AutocompleteMatch AutoCompletePopupModelTest::CreateMatch(
    const string16& keyword,
    const string16& query_string,
    AutocompleteMatch::Type type) {
  AutocompleteMatch match(NULL, 0, false, type);
  match.contents = query_string;
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_.get());
  if (!keyword.empty()) {
    const TemplateURL* template_url =
        template_url_service->GetTemplateURLForKeyword(keyword);
    match.template_url =
        TemplateURL::SupportsReplacement(template_url) ? template_url : NULL;
  }
  if (match.template_url)
    match.fill_into_edit = match.template_url->keyword() + char16(' ');
  else
    match.template_url = template_url_service->GetDefaultSearchProvider();
  match.fill_into_edit.append(query_string);
  match.transition = keyword.empty() ?
      PageTransition::GENERATED : PageTransition::KEYWORD;
  return match;
}

void AutoCompletePopupModelTest::RunTest(const char* input,
                                         AutocompleteMatch::Type type,
                                         const char* keyword) {
  string16 keyword16(ASCIIToUTF16(keyword));
  string16 detected_keyword;
  EXPECT_FALSE(model_->GetKeywordForMatch(
    CreateMatch(keyword16, ASCIIToUTF16(input), type), &detected_keyword));
  EXPECT_EQ(keyword16, detected_keyword);
}

TEST_F(AutoCompletePopupModelTest, GetKeywordForMatch) {
  string16 keyword;

  // Possible matches when the input is "tfoo"
  RunTest("tfoo", AutocompleteMatch::SEARCH_WHAT_YOU_TYPED, "");
  RunTest("tfoo", AutocompleteMatch::SEARCH_HISTORY, "");
  RunTest("tfoo", AutocompleteMatch::SEARCH_SUGGEST, "");

  // Possible matches when the input is "t foo"
  RunTest("foo", AutocompleteMatch::SEARCH_HISTORY, "t");
  RunTest("foo", AutocompleteMatch::SEARCH_OTHER_ENGINE, "t");

  // Possible matches when the input is "k foo"
  RunTest("foo", AutocompleteMatch::SEARCH_HISTORY, "k");
  RunTest("foo", AutocompleteMatch::SEARCH_OTHER_ENGINE, "k");
}
