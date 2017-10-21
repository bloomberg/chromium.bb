// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/physical_web_provider.h"

#include <memory>
#include <string>

#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/physical_web/data_source/fake_physical_web_data_source.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/data_source/physical_web_listener.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

using physical_web::FakePhysicalWebDataSource;
using physical_web::PhysicalWebDataSource;
using physical_web::PhysicalWebListener;

namespace {

// An autocomplete provider client that embeds the fake Physical Web data
// source.
class FakeAutocompleteProviderClient
    : public testing::NiceMock<MockAutocompleteProviderClient> {
 public:
  FakeAutocompleteProviderClient()
      : physical_web_data_source_(
            base::MakeUnique<FakePhysicalWebDataSource>()),
        is_off_the_record_(false) {}

  const AutocompleteSchemeClassifier& GetSchemeClassifier() const override {
    return scheme_classifier_;
  }

  PhysicalWebDataSource* GetPhysicalWebDataSource() override {
    return physical_web_data_source_.get();
  }

  bool IsOffTheRecord() const override {
    return is_off_the_record_;
  }

  // Convenience method to avoid downcasts when accessing the fake data source.
  FakePhysicalWebDataSource* GetFakePhysicalWebDataSource() {
    return physical_web_data_source_.get();
  }

  // Allow tests to enable incognito mode.
  void SetOffTheRecord(bool is_off_the_record) {
    is_off_the_record_ = is_off_the_record;
  }

 private:
  std::unique_ptr<FakePhysicalWebDataSource> physical_web_data_source_;
  TestSchemeClassifier scheme_classifier_;
  bool is_off_the_record_;
};

}  // namespace

class PhysicalWebProviderTest : public testing::Test {
 protected:
  PhysicalWebProviderTest() : provider_(NULL) {
    ResetFieldTrialList();
  }

  ~PhysicalWebProviderTest() override {}

  void SetUp() override {
    base::FieldTrial* trial = CreatePhysicalWebFieldTrial();
    trial->group();
    client_.reset(new FakeAutocompleteProviderClient());
    provider_ = PhysicalWebProvider::Create(client_.get(), nullptr);
  }

  void TearDown() override {
    provider_ = NULL;
  }

  void ResetFieldTrialList() {
    // Destroy the existing FieldTrialList before creating a new one to avoid a
    // DCHECK.
    field_trial_list_.reset();
    field_trial_list_.reset(new base::FieldTrialList(
        base::MakeUnique<metrics::SHA1EntropyProvider>("foo")));
    variations::testing::ClearAllVariationParams();
  }

  static base::FieldTrial* CreatePhysicalWebFieldTrial() {
    std::map<std::string, std::string> params;
    params[OmniboxFieldTrial::kPhysicalWebZeroSuggestRule] = "true";
    params[OmniboxFieldTrial::kPhysicalWebAfterTypingRule] = "true";
    variations::AssociateVariationParams(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A", params);
    return base::FieldTrialList::CreateFieldTrial(
        OmniboxFieldTrial::kBundledExperimentFieldTrialName, "A");
  }

  // Create a dummy metadata list with |metadata_count| items. Each item is
  // populated with a unique scanned URL and page metadata.
  static std::unique_ptr<physical_web::MetadataList> CreateMetadata(
      size_t metadata_count) {
    auto metadata_list = base::MakeUnique<physical_web::MetadataList>();
    for (size_t i = 0; i < metadata_count; ++i) {
      std::string item_id = base::SizeTToString(i);
      std::string url = "https://example.com/" + item_id;
      metadata_list->emplace_back();
      auto& metadata_item = metadata_list->back();
      metadata_item.scanned_url = GURL(url);
      metadata_item.resolved_url = GURL(url);
      metadata_item.icon_url = GURL(url);
      metadata_item.title = "Example title " + item_id;
      metadata_item.description = "Example description " + item_id;
    }
    return metadata_list;
  }

  // Construct an AutocompleteInput to represent tapping the omnibox from the
  // new tab page.
  static AutocompleteInput CreateInputForNTP() {
    AutocompleteInput input(
        base::string16(),
        metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
        TestSchemeClassifier());
    input.set_from_omnibox_focus(true);
    return input;
  }

  // Construct an AutocompleteInput to represent tapping the omnibox with |url|
  // as the current web page.
  static AutocompleteInput CreateInputWithCurrentUrl(const std::string& url) {
    AutocompleteInput input(base::UTF8ToUTF16(url),
                            metrics::OmniboxEventProto::OTHER,
                            TestSchemeClassifier());
    input.set_current_url(GURL(url));
    input.set_from_omnibox_focus(true);
    return input;
  }

  // For a given |match|, check that the destination URL, contents string,
  // description string, and default match state agree with the values specified
  // in |url|, |contents|, |description|, and |allowed_to_be_default_match|.
  static void ValidateMatch(const AutocompleteMatch& match,
                            const std::string& url,
                            const std::string& contents,
                            const std::string& description,
                            bool allowed_to_be_default_match) {
    EXPECT_EQ(url, match.destination_url.spec());
    EXPECT_EQ(contents, base::UTF16ToUTF8(match.contents));
    EXPECT_EQ(description, base::UTF16ToUTF8(match.description));
    EXPECT_EQ(allowed_to_be_default_match, match.allowed_to_be_default_match);
  }

  // Returns the contents string for an overflow item. Use |truncated_title|
  // as the title of the first match, |match_count_without_default| as the
  // total number of matches (not counting the default match), and
  // |metadata_count| as the number of nearby Physical Web URLs for which we
  // have metadata.
  static std::string ConstructOverflowItemContents(
      const std::string& truncated_title,
      size_t match_count_without_default,
      size_t metadata_count) {
    // Don't treat the overflow item as a metadata match.
    const size_t metadata_match_count = match_count_without_default - 1;
    // Determine how many URLs we didn't create match items for.
    const size_t additional_url_count = metadata_count - metadata_match_count;

    // Build the contents string.
    if (truncated_title.empty()) {
      return l10n_util::GetPluralStringFUTF8(
          IDS_PHYSICAL_WEB_OVERFLOW_EMPTY_TITLE, additional_url_count);
    } else {
      // Subtract one from the additional URL count because the first item is
      // represented by its title.
      std::string contents_suffix = l10n_util::GetPluralStringFUTF8(
          IDS_PHYSICAL_WEB_OVERFLOW, additional_url_count - 1);
      return truncated_title + " " + contents_suffix;
    }
  }

  // Run a test case using |input| as the simulated state of the omnibox input
  // field, |metadata_list| as the list of simulated Physical Web metadata,
  // and |title_truncated| as the truncated title of the first match. In
  // addition to checking the fields of the overflow item, this will also check
  // that the total number of matches is equal to |expected_match_count| and
  // that a default match and overflow item are only present when
  // |should_expect_default_match| or |should_expect_overflow_item| are true.
  // Metadata matches are not checked.
  void OverflowItemTestCase(
      const AutocompleteInput& input,
      std::unique_ptr<physical_web::MetadataList> metadata_list,
      const std::string& title_truncated,
      size_t expected_match_count,
      bool should_expect_default_match,
      bool should_expect_overflow_item) {
    const size_t metadata_count = metadata_list->size();

    FakePhysicalWebDataSource* data_source =
        client_->GetFakePhysicalWebDataSource();
    EXPECT_TRUE(data_source);

    data_source->SetMetadataList(std::move(metadata_list));

    provider_->Start(input, false);

    const size_t match_count = provider_->matches().size();
    EXPECT_EQ(expected_match_count, match_count);

    const size_t match_count_without_default =
        should_expect_default_match ? match_count - 1 : match_count;

    if (should_expect_overflow_item) {
      EXPECT_LT(match_count_without_default, metadata_count);
    } else {
      EXPECT_EQ(match_count_without_default, metadata_count);
    }

    size_t overflow_match_count = 0;
    size_t default_match_count = 0;
    for (const auto& match : provider_->matches()) {
      if (match.type == AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW) {
        std::string contents = ConstructOverflowItemContents(
            title_truncated, match_count_without_default, metadata_count);
        ValidateMatch(
            match, "chrome://physical-web/", contents,
            l10n_util::GetStringUTF8(IDS_PHYSICAL_WEB_OVERFLOW_DESCRIPTION),
            false);
        ++overflow_match_count;
      } else if (match.allowed_to_be_default_match) {
        ++default_match_count;
      }
    }
    EXPECT_EQ(should_expect_overflow_item ? 1U : 0U, overflow_match_count);
    EXPECT_EQ(should_expect_default_match ? 1U : 0U, default_match_count);
  }

  std::unique_ptr<FakeAutocompleteProviderClient> client_;
  scoped_refptr<PhysicalWebProvider> provider_;
  std::unique_ptr<base::FieldTrialList> field_trial_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebProviderTest);
};

TEST_F(PhysicalWebProviderTest, TestEmptyMetadataListCreatesNoMatches) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadataList(CreateMetadata(0));

  // Run the test with no text in the omnibox input to simulate NTP.
  provider_->Start(CreateInputForNTP(), false);
  EXPECT_TRUE(provider_->matches().empty());

  // Run the test again with a URL in the omnibox input.
  provider_->Start(CreateInputWithCurrentUrl("http://www.cnn.com"), false);
  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(PhysicalWebProviderTest, TestSingleMetadataItemCreatesOneMatch) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  // Extract the URL and title before inserting the metadata into the data
  // source.
  auto metadata_list = CreateMetadata(1);
  const auto& metadata_item = metadata_list->front();
  std::string resolved_url = metadata_item.resolved_url.spec();
  std::string title = metadata_item.title;

  data_source->SetMetadataList(std::move(metadata_list));

  // Run the test with no text in the omnibox input to simulate NTP.
  provider_->Start(CreateInputForNTP(), false);

  // Check that there is only one match item and its fields are correct.
  EXPECT_EQ(1U, provider_->matches().size());
  const AutocompleteMatch& metadata_match = provider_->matches().front();
  EXPECT_EQ(AutocompleteMatchType::PHYSICAL_WEB, metadata_match.type);
  ValidateMatch(metadata_match, resolved_url, resolved_url, title, false);

  // Run the test again with a URL in the omnibox input. An additional match
  // should be added as a default match.
  provider_->Start(CreateInputWithCurrentUrl("http://www.cnn.com"), false);

  size_t metadata_match_count = 0;
  size_t default_match_count = 0;
  for (const auto& match : provider_->matches()) {
    if (match.type == AutocompleteMatchType::PHYSICAL_WEB) {
      ValidateMatch(match, resolved_url, resolved_url, title, false);
      ++metadata_match_count;
    } else {
      EXPECT_TRUE(match.allowed_to_be_default_match);
      ++default_match_count;
    }
  }
  EXPECT_EQ(2U, provider_->matches().size());
  EXPECT_EQ(1U, metadata_match_count);
  EXPECT_EQ(1U, default_match_count);
}

TEST_F(PhysicalWebProviderTest, TestNoMatchesWithUserInput) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadataList(CreateMetadata(1));

  // Construct an AutocompleteInput to simulate user input in the omnibox input
  // field. The provider should not generate any matches.
  std::string text("user input");
  AutocompleteInput input(
      base::UTF8ToUTF16(text), text.length(),
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());
  provider_->Start(input, false);

  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(PhysicalWebProviderTest, TestEmptyInputAfterTyping) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadataList(CreateMetadata(1));

  // Construct an AutocompleteInput to simulate a blank input field, as if the
  // user typed a query and then deleted it. The provider should generate
  // suggestions for the zero-suggest case. No default match should be created.
  AutocompleteInput input(
      base::string16(), 0,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());
  provider_->Start(input, false);

  size_t metadata_match_count = 0;
  size_t default_match_count = 0;
  for (const auto& match : provider_->matches()) {
    if (match.type == AutocompleteMatchType::PHYSICAL_WEB) {
      ++metadata_match_count;
    } else {
      EXPECT_TRUE(match.allowed_to_be_default_match);
      ++default_match_count;
    }
  }
  EXPECT_EQ(1U, provider_->matches().size());
  EXPECT_EQ(1U, metadata_match_count);
  EXPECT_EQ(0U, default_match_count);
}

TEST_F(PhysicalWebProviderTest, TestManyMetadataItemsCreatesOverflowItem) {
  // Create enough metadata to guarantee an overflow item will be created.
  const size_t metadata_count = AutocompleteProvider::kMaxMatches + 1;

  // Run the test with no text in the omnibox input to simulate NTP.
  OverflowItemTestCase(
      CreateInputForNTP(), CreateMetadata(metadata_count), "Example title 0",
      PhysicalWebProvider::kPhysicalWebMaxMatches, false, true);

  // Run the test again with a URL in the omnibox input. An additional match
  // should be added as a default match.
  OverflowItemTestCase(CreateInputWithCurrentUrl("http://www.cnn.com"),
                       CreateMetadata(metadata_count), "Example title 0",
                       PhysicalWebProvider::kPhysicalWebMaxMatches + 1, true,
                       true);
}

TEST_F(PhysicalWebProviderTest, TestLongPageTitleIsTruncatedInOverflowItem) {
  // Set a long title for the first item. The page title for this item will
  // appear in the overflow item's content string.
  auto metadata_list = CreateMetadata(AutocompleteProvider::kMaxMatches + 1);
  auto& metadata_item = metadata_list->front();
  metadata_item.title = "Extra long example title 0";

  OverflowItemTestCase(CreateInputForNTP(), std::move(metadata_list),
                       "Extra long exa" + std::string(gfx::kEllipsis),
                       PhysicalWebProvider::kPhysicalWebMaxMatches, false,
                       true);
}

TEST_F(PhysicalWebProviderTest, TestEmptyPageTitleInOverflowItem) {
  // Set an empty title for the first item. Because the title is empty, we will
  // display an alternate string in the overflow item's contents.
  auto metadata_list = CreateMetadata(AutocompleteProvider::kMaxMatches + 1);
  auto& metadata_item = metadata_list->front();
  metadata_item.title = "";

  OverflowItemTestCase(CreateInputForNTP(), std::move(metadata_list), "",
                       PhysicalWebProvider::kPhysicalWebMaxMatches, false,
                       true);
}

TEST_F(PhysicalWebProviderTest, TestRTLPageTitleInOverflowItem) {
  // Set a Hebrew title for the first item.
  auto metadata_list = CreateMetadata(AutocompleteProvider::kMaxMatches + 1);
  auto& metadata_item = metadata_list->front();
  metadata_item.title = "ויקיפדיה";

  OverflowItemTestCase(CreateInputForNTP(), std::move(metadata_list),
                       "ויקיפדיה", PhysicalWebProvider::kPhysicalWebMaxMatches,
                       false, true);
}

TEST_F(PhysicalWebProviderTest, TestNoMatchesInIncognito) {
  // Enable incognito mode
  client_->SetOffTheRecord(true);

  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  data_source->SetMetadataList(CreateMetadata(1));
  provider_->Start(CreateInputForNTP(), false);

  EXPECT_TRUE(provider_->matches().empty());
}

TEST_F(PhysicalWebProviderTest, TestNearbyURLCountHistograms) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  AutocompleteInput zero_suggest_input(CreateInputForNTP());
  AutocompleteInput after_typing_input(
      base::UTF8ToUTF16("Example"), 7,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());

  data_source->SetMetadataList(CreateMetadata(3));

  {
    // Simulate the user selecting a suggestion when the omnibox is empty
    // (zero suggest case). Both histograms should record the same counts.
    base::HistogramTester histogram_tester;
    ProvidersInfo provider_info;

    provider_->Start(zero_suggest_input, false);
    provider_->AddProviderInfo(&provider_info);

    histogram_tester.ExpectUniqueSample(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtMatchCreation", 3, 1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtFocus", 3, 1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.PhysicalWebProvider.SuggestionUsedWithoutOmniboxFocus", 0, 1);
  }

  {
    // Simulate the user selecting a suggestion after typing a query (after
    // typing case). Some additional URLs are discovered between focusing the
    // omnibox and typing a query, so the counts should differ.
    base::HistogramTester histogram_tester;
    ProvidersInfo provider_info;

    provider_->Start(zero_suggest_input, false);
    data_source->SetMetadataList(CreateMetadata(6));
    provider_->Start(after_typing_input, false);
    provider_->AddProviderInfo(&provider_info);

    histogram_tester.ExpectUniqueSample(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtMatchCreation", 6, 1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtFocus", 3, 1);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.PhysicalWebProvider.SuggestionUsedWithoutOmniboxFocus", 0, 1);
  }
}

TEST_F(PhysicalWebProviderTest, TestNearbyURLCountAfterTypingWithoutFocus) {
  FakePhysicalWebDataSource* data_source =
      client_->GetFakePhysicalWebDataSource();
  EXPECT_TRUE(data_source);

  AutocompleteInput after_typing_input(
      base::UTF8ToUTF16("Example"), 7,
      metrics::OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
      TestSchemeClassifier());

  data_source->SetMetadataList(CreateMetadata(3));

  {
    // Simulate selecting a suggestion without focusing the omnibox first.
    base::HistogramTester histogram_tester;
    ProvidersInfo provider_info;

    provider_->Start(after_typing_input, false);
    provider_->AddProviderInfo(&provider_info);

    histogram_tester.ExpectUniqueSample(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtMatchCreation", 3, 1);
    histogram_tester.ExpectTotalCount(
        "Omnibox.SuggestionUsed.NearbyURLCount.AtFocus", 0);
    histogram_tester.ExpectUniqueSample(
        "Omnibox.PhysicalWebProvider.SuggestionUsedWithoutOmniboxFocus", 1, 1);
  }
}
