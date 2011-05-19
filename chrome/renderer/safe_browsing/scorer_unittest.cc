// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/safe_browsing/scorer.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/thread.h"
#include "chrome/renderer/safe_browsing/client_model.pb.h"
#include "chrome/renderer/safe_browsing/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class PhishingScorerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    // Setup a simple model.  Note that the scorer does not care about
    // how features are encoded so we use readable strings here to make
    // the test simpler to follow.
    model_.Clear();
    model_.add_hashes("feature1");
    model_.add_hashes("feature2");
    model_.add_hashes("feature3");
    model_.add_hashes("token one");
    model_.add_hashes("token two");
    model_.add_hashes("token");
    model_.add_hashes("one");
    model_.add_hashes("two");

    ClientSideModel::Rule* rule;
    rule = model_.add_rule();
    rule->set_weight(0.5);

    rule = model_.add_rule();
    rule->add_feature(0);  // feature1
    rule->set_weight(2.0);

    rule = model_.add_rule();
    rule->add_feature(0);  // feature1
    rule->add_feature(1);  // feature2
    rule->set_weight(3.0);

    model_.add_page_term(3);  // token one
    model_.add_page_term(4);  // token two

    model_.add_page_word(5);  // token
    model_.add_page_word(6);  // one
    model_.add_page_word(7);  // two

    model_.set_max_words_per_term(2);
  }

  // Helper to write the given data to a file and then open the file for
  // reading.  Returns kInvalidPlatformFileValue on error.
  base::PlatformFile WriteAndOpenModelFile(
      FilePath model_file_path,
      const std::string& model_data) {
    if (file_util::WriteFile(
            model_file_path,
            model_data.data(),
            model_data.size()) != static_cast<int>(model_data.size())) {
      LOG(ERROR) << "Unable to write model data to file";
      return base::kInvalidPlatformFileValue;
    }

    return base::CreatePlatformFile(
        model_file_path,
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL,   // created
        NULL);  // error_code
  }

  // Helper to test CreateFromFile.  Calls the method, then runs the
  // message loop if necessary until the load finishes.  Returns the
  // callback result.
  Scorer* CreateFromFile(
      base::PlatformFile model_file,
      scoped_refptr<base::MessageLoopProxy> file_thread_proxy) {
    callback_result_ = NULL;
    Scorer::CreateFromFile(
        model_file,
        file_thread_proxy,
        NewCallback(this, &PhishingScorerTest::PhishingScorerCreated));
    message_loop_.Run();
    return callback_result_;
  }

  void PhishingScorerCreated(Scorer* scorer) {
    callback_result_ = scorer;
    message_loop_.Quit();
  }

  // Proxy for private model member variable.
  const ClientSideModel& GetModel(const Scorer& scorer) {
    return scorer.model_;
  }

  ClientSideModel model_;

  // For the CreateFromFile test.
  MessageLoop message_loop_;
  Scorer* callback_result_;
};

TEST_F(PhishingScorerTest, HasValidModel) {
  scoped_ptr<Scorer> scorer;
  scorer.reset(Scorer::Create(model_.SerializeAsString()));
  EXPECT_TRUE(scorer.get() != NULL);

  // Invalid model string.
  scorer.reset(Scorer::Create("bogus string"));
  EXPECT_FALSE(scorer.get());

  // Mode is missing a required field.
  model_.clear_max_words_per_term();
  scorer.reset(Scorer::Create(model_.SerializePartialAsString()));
  EXPECT_FALSE(scorer.get());
}

TEST_F(PhishingScorerTest, CreateFromFile) {
  // Write out the testing model to a file.
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath model_file_path = temp_dir.path().AppendASCII("phishing_model.pb");

  std::string serialized_model = model_.SerializeAsString();
  base::PlatformFile model_file = WriteAndOpenModelFile(
      model_file_path,
      serialized_model);
  ASSERT_NE(base::kInvalidPlatformFileValue, model_file);

  base::Thread loader_thread("PhishingScorerTest::FILE");
  loader_thread.Start();

  scoped_ptr<Scorer> scorer(
      CreateFromFile(model_file, loader_thread.message_loop_proxy()));
  ASSERT_TRUE(scorer.get());
  EXPECT_EQ(serialized_model, GetModel(*scorer).SerializeAsString());
  base::ClosePlatformFile(model_file);

  // Now try with an empty file.
  model_file = WriteAndOpenModelFile(model_file_path, "");
  ASSERT_NE(base::kInvalidPlatformFileValue, model_file);
  scorer.reset(CreateFromFile(model_file, loader_thread.message_loop_proxy()));
  ASSERT_FALSE(scorer.get());
  base::ClosePlatformFile(model_file);

  // Try with a file that's too large.
  model_.add_hashes(std::string(Scorer::kMaxPhishingModelSizeBytes, '0'));
  model_file = WriteAndOpenModelFile(model_file_path,
                                     model_.SerializeAsString());
  ASSERT_NE(base::kInvalidPlatformFileValue, model_file);
  scorer.reset(CreateFromFile(model_file, loader_thread.message_loop_proxy()));
  ASSERT_FALSE(scorer.get());
  base::ClosePlatformFile(model_file);

  // Finally, try with an invalid file.
  scorer.reset(CreateFromFile(base::kInvalidPlatformFileValue,
                              loader_thread.message_loop_proxy()));
  ASSERT_FALSE(scorer.get());
}

TEST_F(PhishingScorerTest, PageTerms) {
  scoped_ptr<Scorer> scorer(Scorer::Create(model_.SerializeAsString()));
  ASSERT_TRUE(scorer.get());
  base::hash_set<std::string> expected_page_terms;
  expected_page_terms.insert("token one");
  expected_page_terms.insert("token two");
  EXPECT_THAT(scorer->page_terms(),
              ::testing::ContainerEq(expected_page_terms));
}

TEST_F(PhishingScorerTest, PageWords) {
  scoped_ptr<Scorer> scorer(Scorer::Create(model_.SerializeAsString()));
  ASSERT_TRUE(scorer.get());
  base::hash_set<std::string> expected_page_words;
  expected_page_words.insert("token");
  expected_page_words.insert("one");
  expected_page_words.insert("two");
  EXPECT_THAT(scorer->page_words(),
              ::testing::ContainerEq(expected_page_words));
}

TEST_F(PhishingScorerTest, ComputeScore) {
  scoped_ptr<Scorer> scorer(Scorer::Create(model_.SerializeAsString()));
  ASSERT_TRUE(scorer.get());

  // An empty feature map should match the empty rule.
  FeatureMap features;
  // The expected logodds is 0.5 (empty rule) => p = exp(0.5) / (exp(0.5) + 1)
  // => 0.62245933120185459
  EXPECT_DOUBLE_EQ(0.62245933120185459, scorer->ComputeScore(features));
  // Same if the feature does not match any rule.
  EXPECT_TRUE(features.AddBooleanFeature("not existing feature"));
  EXPECT_DOUBLE_EQ(0.62245933120185459, scorer->ComputeScore(features));

  // Feature 1 matches which means that the logodds will be:
  //   0.5 (empty rule) + 2.0 (rule weight) * 0.15 (feature weight) = 0.8
  //   => p = 0.6899744811276125
  EXPECT_TRUE(features.AddRealFeature("feature1", 0.15));
  EXPECT_DOUBLE_EQ(0.6899744811276125, scorer->ComputeScore(features));

  // Now, both feature 1 and feature 2 match.  Expected logodds:
  //   0.5 (empty rule) + 2.0 (rule weight) * 0.15 (feature weight) +
  //   3.0 (rule weight) * 0.15 (feature1 weight) * 1.0 (feature2) weight = 9.8
  //   => p = 0.99999627336071584
  EXPECT_TRUE(features.AddBooleanFeature("feature2"));
  EXPECT_DOUBLE_EQ(0.77729986117469119, scorer->ComputeScore(features));
}
}  // namespace safe_browsing
