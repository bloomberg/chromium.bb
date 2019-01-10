// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/clipboard_url_provider.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/open_from_clipboard/fake_clipboard_recent_content.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kCurrentURL[] = "http://example.com/current";
const char kClipboardURL[] = "http://example.com/clipboard";
const char kClipboardText[] = "Search for me";
const char kClipboardTitleText[] = "\"Search for me\"";

class ClipboardURLProviderTest : public testing::Test {
 public:
  ClipboardURLProviderTest()
      : client_(new MockAutocompleteProviderClient()),
        provider_(new ClipboardURLProvider(client_.get(),
                                           nullptr,
                                           &clipboard_content_)) {
    SetClipboardUrl(GURL(kClipboardURL));
  }

  ~ClipboardURLProviderTest() override {}

  void ClearClipboard() { clipboard_content_.SuppressClipboardContent(); }

  void SetClipboardUrl(const GURL& url) {
    clipboard_content_.SetClipboardURL(url, base::TimeDelta::FromMinutes(10));
  }

  void SetClipboardText(const base::string16& text) {
    clipboard_content_.SetClipboardText(text, base::TimeDelta::FromMinutes(10));
  }

  AutocompleteInput CreateAutocompleteInput(bool from_omnibox_focus) {
    AutocompleteInput input(base::string16(), metrics::OmniboxEventProto::OTHER,
                            classifier_);
    input.set_current_url(GURL(kCurrentURL));
    input.set_from_omnibox_focus(from_omnibox_focus);
    return input;
  }

 protected:
  TestSchemeClassifier classifier_;
  FakeClipboardRecentContent clipboard_content_;
  std::unique_ptr<MockAutocompleteProviderClient> client_;
  scoped_refptr<ClipboardURLProvider> provider_;
};

TEST_F(ClipboardURLProviderTest, NotFromOmniboxFocus) {
  provider_->Start(CreateAutocompleteInput(false), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardURLProviderTest, EmptyClipboard) {
  ClearClipboard();
  provider_->Start(CreateAutocompleteInput(true), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardURLProviderTest, ClipboardIsCurrentURL) {
  SetClipboardUrl(GURL(kCurrentURL));
  provider_->Start(CreateAutocompleteInput(true), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(ClipboardURLProviderTest, HasMultipleMatches) {
  provider_->Start(CreateAutocompleteInput(true), false);
  ASSERT_GE(provider_->matches().size(), 1U);
  EXPECT_EQ(GURL(kClipboardURL), provider_->matches().back().destination_url);
}

TEST_F(ClipboardURLProviderTest, MatchesText) {
  base::test::ScopedFeatureList feature_list;
  base::Feature textFeature = omnibox::kEnableClipboardProviderTextSuggestions;
  feature_list.InitAndEnableFeature(textFeature);
  auto template_url_service = std::make_unique<TemplateURLService>(
      /*initializers=*/nullptr, /*count=*/0);
  client_->set_template_url_service(std::move(template_url_service));
  SetClipboardText(base::UTF8ToUTF16(kClipboardText));
  provider_->Start(CreateAutocompleteInput(true), false);
  ASSERT_GE(provider_->matches().size(), 1U);
  EXPECT_EQ(base::UTF8ToUTF16(kClipboardTitleText),
            provider_->matches().back().contents);
}

}  // namespace
