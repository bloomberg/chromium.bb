// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spell_check_host_impl.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#error !BUILDFLAG(USE_BROWSER_SPELLCHECKER) is required for these tests.
#endif

class TestSpellCheckHostImpl {
 public:
  TestSpellCheckHostImpl()
      : spellcheck_(base::MakeUnique<SpellcheckService>(&testing_profile_)) {}

  SpellcheckCustomDictionary& GetCustomDictionary() const {
    EXPECT_NE(nullptr, spellcheck_.get());
    SpellcheckCustomDictionary* custom_dictionary =
        spellcheck_->GetCustomDictionary();
    return *custom_dictionary;
  }

  std::vector<SpellCheckResult> FilterCustomWordResults(
      const std::string& text,
      const std::vector<SpellCheckResult>& service_results) const {
    return SpellCheckHostImpl::FilterCustomWordResults(
        text, GetCustomDictionary(), service_results);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile testing_profile_;
  std::unique_ptr<SpellcheckService> spellcheck_;

  DISALLOW_COPY_AND_ASSIGN(TestSpellCheckHostImpl);
};

// Spelling corrections of custom dictionary words should be removed from the
// results returned by the remote Spelling service.
TEST(SpellCheckHostImplTest, CustomSpellingResults) {
  std::vector<SpellCheckResult> service_results;
  service_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 0, 6,
                                             base::ASCIIToUTF16("Hello")));
  service_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 7, 5,
                                             base::ASCIIToUTF16("World")));
  TestSpellCheckHostImpl host_impl;
  host_impl.GetCustomDictionary().AddWord("Helllo");
  std::vector<SpellCheckResult> results =
      host_impl.FilterCustomWordResults("Helllo Warld", service_results);
  ASSERT_EQ(1u, results.size());

  EXPECT_EQ(service_results[1].decoration, results[0].decoration);
  EXPECT_EQ(service_results[1].location, results[0].location);
  EXPECT_EQ(service_results[1].length, results[0].length);
  EXPECT_EQ(service_results[1].replacements.size(),
            results[0].replacements.size());
  EXPECT_EQ(service_results[1].replacements[0], results[0].replacements[0]);
}

// Spelling corrections of words that are not in the custom dictionary should
// be retained in the results returned by the remote Spelling service.
TEST(SpellCheckHostImplTest, SpellingServiceResults) {
  std::vector<SpellCheckResult> service_results;
  service_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 0, 6,
                                             base::ASCIIToUTF16("Hello")));
  service_results.push_back(SpellCheckResult(SpellCheckResult::SPELLING, 7, 5,
                                             base::ASCIIToUTF16("World")));
  TestSpellCheckHostImpl host_impl;
  host_impl.GetCustomDictionary().AddWord("Hulo");
  std::vector<SpellCheckResult> results =
      host_impl.FilterCustomWordResults("Helllo Warld", service_results);
  ASSERT_EQ(service_results.size(), results.size());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(service_results[i].decoration, results[i].decoration);
    EXPECT_EQ(service_results[i].location, results[i].location);
    EXPECT_EQ(service_results[i].length, results[i].length);
    EXPECT_EQ(service_results[i].replacements.size(),
              results[i].replacements.size());
    EXPECT_EQ(service_results[i].replacements[0], results[i].replacements[0]);
  }
}
