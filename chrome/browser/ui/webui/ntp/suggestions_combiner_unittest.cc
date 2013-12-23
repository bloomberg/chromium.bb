// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(beaudoin): What is really needed here?

#include <deque>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ntp/suggestions_combiner.h"
#include "chrome/browser/ui/webui/ntp/suggestions_page_handler.h"
#include "chrome/browser/ui/webui/ntp/suggestions_source.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct SourceInfo {
  int weight;
  const char* source_name;
  int number_of_suggestions;
};

struct TestDescription {
  SourceInfo sources[3];
  const char* results[8];
} test_suite[] = {
  // One source, more than 8 items.
  {
    {{1, "A", 10}},
    {"A 0", "A 1", "A 2", "A 3", "A 4", "A 5", "A 6", "A 7"}
  },
  // One source, exactly 8 items.
  {
    {{1, "A", 8}},
    {"A 0", "A 1", "A 2", "A 3", "A 4", "A 5", "A 6", "A 7"}
  },
  // One source, not enough items.
  {
    {{1, "A", 3}},
    {"A 0", "A 1", "A 2"}
  },
  // One source, no items.
  {
    {{1, "A", 0}},
    {}
  },
  // Two sources, equal weight, more than 8 items.
  {
    {{1, "A", 10}, {1, "B", 10}},
    {"A 0", "A 1", "A 2", "A 3", "B 0", "B 1", "B 2", "B 3"}
  },
  // Two sources, equal weight, exactly 8 items.
  {
    {{1, "A", 4}, {1, "B", 4}},
    {"A 0", "A 1", "A 2", "A 3", "B 0", "B 1", "B 2", "B 3"}
  },
  // Two sources, equal weight, exactly 8 items but source A has more.
  {
    {{1, "A", 5}, {1, "B", 3}},
    {"A 0", "A 1", "A 2", "A 3", "A 4", "B 0", "B 1", "B 2"}
  },
  // Two sources, equal weight, exactly 8 items but source B has more.
  {
    {{1, "A", 2}, {1, "B", 6}},
    {"A 0", "A 1", "B 0", "B 1", "B 2", "B 3", "B 4", "B 5"}
  },
  // Two sources, equal weight, exactly 8 items but source A has none.
  {
    {{1, "A", 0}, {1, "B", 8}},
    {"B 0", "B 1", "B 2", "B 3", "B 4", "B 5", "B 6", "B 7"}
  },
  // Two sources, equal weight, exactly 8 items but source B has none.
  {
    {{1, "A", 8}, {1, "B", 0}},
    {"A 0", "A 1", "A 2", "A 3", "A 4", "A 5", "A 6", "A 7"}
  },
  // Two sources, equal weight, less than 8 items.
  {
    {{1, "A", 3}, {1, "B", 3}},
    {"A 0", "A 1", "A 2", "B 0", "B 1", "B 2"}
  },
  // Two sources, equal weight, less than 8 items but source A has more.
  {
    {{1, "A", 4}, {1, "B", 3}},
    {"A 0", "A 1", "A 2", "A 3", "B 0", "B 1", "B 2"}
  },
  // Two sources, equal weight, less than 8 items but source B has more.
  {
    {{1, "A", 1}, {1, "B", 3}},
    {"A 0", "B 0", "B 1", "B 2"}
  },
  // Two sources, weights 3/4 A  1/4 B, more than 8 items.
  {
    {{3, "A", 10}, {1, "B", 10}},
    {"A 0", "A 1", "A 2", "A 3", "A 4", "A 5", "B 0", "B 1"}
  },
  // Two sources, weights 1/8 A  7/8 B, more than 8 items.
  {
    {{1, "A", 10}, {7, "B", 10}},
    {"A 0", "B 0", "B 1", "B 2", "B 3", "B 4", "B 5", "B 6"}
  },
  // Two sources, weights 1/3 A  2/3 B, more than 8 items.
  {
    {{1, "A", 10}, {2, "B", 10}},
    {"A 0", "A 1", "B 0", "B 1", "B 2", "B 3", "B 4", "B 5"}
  },
  // Three sources, weights 1/2 A  1/4 B  1/4 C, more than 8 items.
  {
    {{2, "A", 10}, {1, "B", 10}, {1, "C", 10}},
    {"A 0", "A 1", "A 2", "A 3", "B 0", "B 1", "C 0", "C 1"}
  },
  // Three sources, weights 1/3 A  1/3 B  1/3 C, more than 8 items.
  {
    {{1, "A", 10}, {1, "B", 10}, {1, "C", 10}},
    {"A 0", "A 1", "B 0", "B 1", "B 2", "C 0", "C 1", "C 2"}
  },
  // Extra items should be grouped together.
  {
    {{1, "A", 3}, {1, "B", 4}, {10, "C", 1}},
    {"A 0", "A 1", "A 2", "B 0", "B 1", "B 2", "B 3", "C 0"}
  }
};

}  // namespace

// Stub for a SuggestionsSource that can provide a number of fake suggestions.
// Fake suggestions are DictionaryValue with a single "title" string field
// containing the |source_name| followed by the index of the suggestion.
// Not in the empty namespace since it's a friend of SuggestionsCombiner.
class SuggestionsSourceStub : public SuggestionsSource {
 public:
  explicit SuggestionsSourceStub(int weight,
      const std::string& source_name, int number_of_suggestions)
      : combiner_(NULL),
        weight_(weight),
        source_name_(source_name),
        number_of_suggestions_(number_of_suggestions),
        debug_(false) {
  }
  virtual ~SuggestionsSourceStub() {
    STLDeleteElements(&items_);
  }

  // Call this method to simulate that the SuggestionsSource has received all
  // its suggestions.
  void Done() {
    combiner_->OnItemsReady();
  }

 private:
  // SuggestionsSource Override and implementation.
  virtual void SetDebug(bool enable) OVERRIDE {
    debug_ = enable;
  }
  virtual int GetWeight() OVERRIDE {
    return weight_;
  }
  virtual int GetItemCount() OVERRIDE {
    return items_.size();
  }
  virtual base::DictionaryValue* PopItem() OVERRIDE {
    if (items_.empty())
      return NULL;
    base::DictionaryValue* item = items_.front();
    items_.pop_front();
    return item;
  }

  virtual void FetchItems(Profile* profile) OVERRIDE {
    char num_str[21];  // Enough to hold all numbers up to 64-bits.
    for (int i = 0; i < number_of_suggestions_; ++i) {
      base::snprintf(num_str, sizeof(num_str), "%d", i);
      AddSuggestion(source_name_ + ' ' + num_str);
    }
  }

  // Adds a fake suggestion. This suggestion is a DictionaryValue with a single
  // "title" field containing |title|.
  void AddSuggestion(const std::string& title) {
    base::DictionaryValue* item = new base::DictionaryValue();
    item->SetString("title", title);
    items_.push_back(item);
  }

  virtual void SetCombiner(SuggestionsCombiner* combiner) OVERRIDE {
    DCHECK(!combiner_);
    combiner_ = combiner;
  }

  // Our combiner.
  SuggestionsCombiner* combiner_;

  int weight_;
  std::string source_name_;
  int number_of_suggestions_;
  bool debug_;

  // Keep the results of the db query here.
  std::deque<base::DictionaryValue*> items_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceStub);
};

class SuggestionsCombinerTest : public testing::Test {
 public:
  SuggestionsCombinerTest() {
  }

 protected:
  Profile* profile_;
  SuggestionsHandler* suggestions_handler_;
  SuggestionsCombiner* combiner_;

  void Reset() {
    delete combiner_;
    combiner_ = new SuggestionsCombiner(suggestions_handler_, profile_);
  }

 private:
  virtual void SetUp() {
    profile_ = new TestingProfile();
    suggestions_handler_ = new SuggestionsHandler();
    combiner_ = new SuggestionsCombiner(suggestions_handler_, profile_);
  }

  virtual void TearDown() {
    delete combiner_;
    delete suggestions_handler_;
    delete profile_;
  }

  DISALLOW_COPY_AND_ASSIGN(SuggestionsCombinerTest);
};

TEST_F(SuggestionsCombinerTest, NoSource) {
  combiner_->FetchItems(NULL);
  EXPECT_EQ(0UL, combiner_->GetPageValues()->GetSize());
}

TEST_F(SuggestionsCombinerTest, SourcesAreNotDoneFetching) {
  combiner_->AddSource(new SuggestionsSourceStub(1, "sourceA", 10));
  combiner_->AddSource(new SuggestionsSourceStub(1, "sourceB", 10));
  combiner_->FetchItems(NULL);
  EXPECT_EQ(0UL, combiner_->GetPageValues()->GetSize());
}

TEST_F(SuggestionsCombinerTest, TestSuite) {
  size_t test_count = arraysize(test_suite);
  for (size_t i = 0; i < test_count; ++i) {
    const TestDescription& description = test_suite[i];
    size_t source_count = arraysize(description.sources);

    scoped_ptr<SuggestionsSourceStub*[]> sources(
        new SuggestionsSourceStub*[source_count]);

    // Setup sources.
    for (size_t j = 0; j < source_count; ++j) {
      const SourceInfo& source_info = description.sources[j];
      // A NULL |source_name| means we shouldn't add this source.
      if (source_info.source_name) {
        sources[j] = new SuggestionsSourceStub(source_info.weight,
            source_info.source_name, source_info.number_of_suggestions);
        combiner_->AddSource(sources[j]);
      } else {
        sources[j] = NULL;
      }
    }

    // Start fetching.
    combiner_->FetchItems(NULL);

    // Sources complete.
    for (size_t j = 0; j < source_count; ++j) {
      if (sources[j])
        sources[j]->Done();
    }

    // Verify expectations.
    base::ListValue* results = combiner_->GetPageValues();
    size_t result_count = results->GetSize();
    EXPECT_LE(result_count, 8UL);
    for (size_t j = 0; j < 8; ++j) {
      if (j < result_count) {
        std::string value;
        base::DictionaryValue* dictionary;
        results->GetDictionary(j, &dictionary);
        dictionary->GetString("title", &value);
        EXPECT_STREQ(description.results[j], value.c_str()) <<
            " test index:" << i;
      } else {
        EXPECT_EQ(description.results[j], static_cast<const char*>(NULL)) <<
            " test index:" << i;
      }
    }

    Reset();
  }
}

