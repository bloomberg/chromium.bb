// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/phishing_term_feature_extractor.h"

#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "crypto/sha2.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "chrome/renderer/safe_browsing/mock_feature_extractor_clock.h"
#include "chrome/renderer/safe_browsing/murmurhash3_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainerEq;
using ::testing::Return;

namespace safe_browsing {

class PhishingTermFeatureExtractorTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    base::hash_set<std::string> terms;
    terms.insert("one");
    terms.insert("one one");
    terms.insert("two");
    terms.insert("multi word test");
    terms.insert("capitalization");
    terms.insert("space");
    terms.insert("separator");
    terms.insert("punctuation");
    // Chinese (translation of "hello")
    terms.insert("\xe4\xbd\xa0\xe5\xa5\xbd");
    // Chinese (translation of "goodbye")
    terms.insert("\xe5\x86\x8d\xe8\xa7\x81");

    for (base::hash_set<std::string>::iterator it = terms.begin();
         it != terms.end(); ++it) {
      term_hashes_.insert(crypto::SHA256HashString(*it));
    }

    base::hash_set<std::string> words;
    words.insert("one");
    words.insert("two");
    words.insert("multi");
    words.insert("word");
    words.insert("test");
    words.insert("capitalization");
    words.insert("space");
    words.insert("separator");
    words.insert("punctuation");
    words.insert("\xe4\xbd\xa0\xe5\xa5\xbd");
    words.insert("\xe5\x86\x8d\xe8\xa7\x81");

    static const uint32 kMurmurHash3Seed = 2777808611U;
    for (base::hash_set<std::string>::iterator it = words.begin();
         it != words.end(); ++it) {
      word_hashes_.insert(MurmurHash3String(*it, kMurmurHash3Seed));
    }

    extractor_.reset(new PhishingTermFeatureExtractor(
        &term_hashes_,
        &word_hashes_,
        3 /* max_words_per_term */,
        kMurmurHash3Seed,
        &clock_));
  }

  // Runs the TermFeatureExtractor on |page_text|, waiting for the
  // completion callback.  Returns the success boolean from the callback.
  bool ExtractFeatures(const string16* page_text, FeatureMap* features) {
    success_ = false;
    extractor_->ExtractFeatures(
        page_text,
        features,
        base::Bind(&PhishingTermFeatureExtractorTest::ExtractionDone,
                   base::Unretained(this)));
    msg_loop_.Run();
    return success_;
  }

  void PartialExtractFeatures(const string16* page_text, FeatureMap* features) {
    extractor_->ExtractFeatures(
        page_text,
        features,
        base::Bind(&PhishingTermFeatureExtractorTest::ExtractionDone,
                   base::Unretained(this)));
    msg_loop_.PostTask(
        FROM_HERE,
        base::Bind(&PhishingTermFeatureExtractorTest::QuitExtraction,
                   base::Unretained(this)));
    msg_loop_.RunAllPending();
  }

  // Completion callback for feature extraction.
  void ExtractionDone(bool success) {
    success_ = success;
    msg_loop_.Quit();
  }

  void QuitExtraction() {
    extractor_->CancelPendingExtraction();
    msg_loop_.Quit();
  }

  MessageLoop msg_loop_;
  MockFeatureExtractorClock clock_;
  scoped_ptr<PhishingTermFeatureExtractor> extractor_;
  base::hash_set<std::string> term_hashes_;
  base::hash_set<uint32> word_hashes_;
  bool success_;  // holds the success value from ExtractFeatures
};

TEST_F(PhishingTermFeatureExtractorTest, ExtractFeatures) {
  // This test doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  string16 page_text = ASCIIToUTF16("blah");
  FeatureMap expected_features;  // initially empty

  FeatureMap features;
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  page_text = ASCIIToUTF16("one one");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one one"));

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  page_text = ASCIIToUTF16("bla bla multi word test bla");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("multi word test"));

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  // This text has all of the words for one of the terms, but they are
  // not in the correct order.
  page_text = ASCIIToUTF16("bla bla test word multi bla");
  expected_features.Clear();

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  page_text = ASCIIToUTF16("Capitalization plus non-space\n"
                           "separator... punctuation!");
  expected_features.Clear();
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("capitalization"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("space"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("separator"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("punctuation"));

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  // Test with empty page text.
  page_text = string16();
  expected_features.Clear();
  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));

  // Chinese translation of the phrase "hello goodbye". This tests that
  // we can correctly separate terms in languages that don't use spaces.
  page_text = UTF8ToUTF16("\xe4\xbd\xa0\xe5\xa5\xbd\xe5\x86\x8d\xe8\xa7\x81");
  expected_features.Clear();
  expected_features.AddBooleanFeature(
      features::kPageTerm + std::string("\xe4\xbd\xa0\xe5\xa5\xbd"));
  expected_features.AddBooleanFeature(
      features::kPageTerm + std::string("\xe5\x86\x8d\xe8\xa7\x81"));

  features.Clear();
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

TEST_F(PhishingTermFeatureExtractorTest, Continuation) {
  // For this test, we'll cause the feature extraction to run multiple
  // iterations by incrementing the clock.

  // This page has a total of 30 words.  For the features to be computed
  // correctly, the extractor has to process the entire string of text.
  string16 page_text(ASCIIToUTF16("one "));
  for (int i = 0; i < 28; ++i) {
    page_text.append(ASCIIToUTF16(StringPrintf("%d ", i)));
  }
  page_text.append(ASCIIToUTF16("two"));

  // Advance the clock 3 ms every 5 words processed, 10 ms between chunks.
  // Note that this assumes kClockCheckGranularity = 5 and
  // kMaxTimePerChunkMs = 10.
  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(3)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(6)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(9)))
      // Time check after the next 5 words.  This is over the chunk
      // time limit, so a continuation task will be posted.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(12)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(22)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(25)))
      // Time check after the next 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(28)))
      // A final check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(30)));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("one"));
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("two"));

  FeatureMap features;
  ASSERT_TRUE(ExtractFeatures(&page_text, &features));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
  // Make sure none of the mock expectations carry over to the next test.
  ::testing::Mock::VerifyAndClearExpectations(&clock_);

  // Now repeat the test with the same text, but advance the clock faster so
  // that the extraction time exceeds the maximum total time for the feature
  // extractor.  Extraction should fail.  Note that this assumes
  // kMaxTotalTimeMs = 500.
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words,
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(300)))
      // Time check at the start of the second chunk of work.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(350)))
      // Time check after the next 5 words.  This is over the limit.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(600)))
      // A final time check for the histograms.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(620)));

  features.Clear();
  EXPECT_FALSE(ExtractFeatures(&page_text, &features));
}

TEST_F(PhishingTermFeatureExtractorTest, PartialExtractionTest) {
  scoped_ptr<string16> page_text(new string16(ASCIIToUTF16("one ")));
  for (int i = 0; i < 28; ++i) {
    page_text->append(ASCIIToUTF16(StringPrintf("%d ", i)));
  }

  base::TimeTicks now = base::TimeTicks::Now();
  EXPECT_CALL(clock_, Now())
      // Time check at the start of extraction.
      .WillOnce(Return(now))
      // Time check at the start of the first chunk of work.
      .WillOnce(Return(now))
      // Time check after the first 5 words.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(7)))
      // Time check after the next 5 words. This should be greater than
      // kMaxTimePerChunkMs so that we stop and schedule extraction for later.
      .WillOnce(Return(now + base::TimeDelta::FromMilliseconds(14)));

  FeatureMap features;
  // Extract first 10 words then stop.
  PartialExtractFeatures(page_text.get(), &features);

  page_text.reset(new string16());
  for (int i = 30; i < 58; ++i) {
    page_text->append(ASCIIToUTF16(StringPrintf("%d ", i)));
  }
  page_text->append(ASCIIToUTF16("multi word test "));
  features.Clear();

  // This part doesn't exercise the extraction timing.
  EXPECT_CALL(clock_, Now()).WillRepeatedly(Return(base::TimeTicks::Now()));

  // Now extract normally and make sure nothing breaks.
  EXPECT_TRUE(ExtractFeatures(page_text.get(), &features));

  FeatureMap expected_features;
  expected_features.AddBooleanFeature(features::kPageTerm +
                                      std::string("multi word test"));
  EXPECT_THAT(features.features(), ContainerEq(expected_features.features()));
}

}  // namespace safe_browsing
