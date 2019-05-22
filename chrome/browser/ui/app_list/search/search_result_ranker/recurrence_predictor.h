// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

using FakePredictorConfig = RecurrenceRankerConfigProto::FakePredictorConfig;
using DefaultPredictorConfig =
    RecurrenceRankerConfigProto::DefaultPredictorConfig;
using ZeroStateFrecencyPredictorConfig =
    RecurrenceRankerConfigProto::ZeroStateFrecencyPredictorConfig;
using ZeroStateHourBinPredictorConfig =
    RecurrenceRankerConfigProto::ZeroStateHourBinPredictorConfig;

// |RecurrencePredictor| is the interface for all predictors used by
// |RecurrenceRanker| to drive rankings. If a predictor has some form of
// serialisation, it should have a corresponding proto in
// |recurrence_predictor.proto|.
class RecurrencePredictor {
 public:
  virtual ~RecurrencePredictor() = default;

  // Train the predictor on an occurrence of |target| coinciding with
  // |condition|. The predictor will collect its own contextual information, eg.
  // time of day, as part of training. Zero-state predictors should use the
  // one-argument version.
  virtual void Train(unsigned int target);
  virtual void Train(unsigned int target, unsigned int condition);

  // Return a map of all known targets to their scores for the given condition
  // under this predictor. Scores must be within the range [0,1]. Zero-state
  // predictors should use the zero-argument version.
  virtual base::flat_map<unsigned int, float> Rank();
  virtual base::flat_map<unsigned int, float> Rank(unsigned int condition);

  virtual void ToProto(RecurrencePredictorProto* proto) const = 0;
  virtual void FromProto(const RecurrencePredictorProto& proto) = 0;
  virtual const char* GetPredictorName() const = 0;
};

// FakePredictor is a simple 'predictor' used for testing. Rank() returns the
// numbers of times each target has been trained on, and does not handle
// conditions.
//
// WARNING: this breaks the guarantees on the range of values a score can take,
// so should not be used for anything except testing.
class FakePredictor : public RecurrencePredictor {
 public:
  explicit FakePredictor(const FakePredictorConfig& config);
  ~FakePredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target) override;
  base::flat_map<unsigned int, float> Rank() override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  base::flat_map<unsigned int, float> counts_;

  DISALLOW_COPY_AND_ASSIGN(FakePredictor);
};

// DefaultPredictor does no work on its own. Using this predictor makes the
// RecurrenceRanker return the scores of its FrecencyStore instead of using a
// predictor.
class DefaultPredictor : public RecurrencePredictor {
 public:
  explicit DefaultPredictor(const DefaultPredictorConfig& config);
  ~DefaultPredictor() override;

  // RecurrencePredictor:
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPredictor);
};

// ZeroStateFrecencyPredictor ranks targets according to their frecency, and
// can only be used for zero-state predictions. This predictor allows for
// frecency-based rankings with different configuration to that of the ranker's
// FrecencyStore. If frecency-based rankings with the same configuration as the
// store are needed, the DefaultPredictor should be used instead.
class ZeroStateFrecencyPredictor : public RecurrencePredictor {
 public:
  explicit ZeroStateFrecencyPredictor(
      const ZeroStateFrecencyPredictorConfig& config);
  ~ZeroStateFrecencyPredictor() override;

  // Records all information about a target: its id and score, along with the
  // number of updates that had occurred when the score was last calculated.
  // This is used for further score updates.
  struct TargetData {
    float last_score = 0.0f;
    int32_t last_num_updates = 0;
  };

  // RecurrencePredictor:
  void Train(unsigned int target) override;
  base::flat_map<unsigned int, float> Rank() override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  // Decay the given target's score according to how many training steps have
  // occurred since last update.
  void DecayScore(TargetData* score);
  // Decay the scores of all targets.
  void DecayAllScores();

  // Controls how quickly scores decay, in other words controls the trade-off
  // between frequency and recency.
  const float decay_coeff_;

  // Number of times the store has been updated.
  unsigned int num_updates_ = 0;

  // This stores all the data of the frecency predictor.
  // TODO(tby): benchmark which map is best in practice for our use.
  base::flat_map<unsigned int, ZeroStateFrecencyPredictor::TargetData> targets_;

  DISALLOW_COPY_AND_ASSIGN(ZeroStateFrecencyPredictor);
};

// |ZeroStateHourBinPredictor| ranks targets according to their frequency during
// the current and neighbor hour bins. It can only be used for zero-state
// predictions.
class ZeroStateHourBinPredictor : public RecurrencePredictor {
 public:
  explicit ZeroStateHourBinPredictor(
      const ZeroStateHourBinPredictorConfig& config);
  ~ZeroStateHourBinPredictor() override;

  // RecurrencePredictor:
  void Train(unsigned int target) override;
  base::flat_map<unsigned int, float> Rank() override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  FRIEND_TEST_ALL_PREFIXES(ZeroStateHourBinPredictorTest, GetTheRightBin);
  FRIEND_TEST_ALL_PREFIXES(ZeroStateHourBinPredictorTest,
                           TrainAndRankSingleBin);
  FRIEND_TEST_ALL_PREFIXES(ZeroStateHourBinPredictorTest,
                           TrainAndRankMultipleBin);
  FRIEND_TEST_ALL_PREFIXES(ZeroStateHourBinPredictorTest, ToProto);
  FRIEND_TEST_ALL_PREFIXES(ZeroStateHourBinPredictorTest, FromProtoDecays);
  // Return the bin index that is |hour_difference| away from the current bin
  // index.
  int GetBinFromHourDifference(int hour_difference) const;
  // Return the current bin index of this predictor.
  int GetBin() const;
  // Check decay condition.
  bool ShouldDecay();
  // Decay the frequency of all items.
  void DecayAll();
  void SetLastDecayTimestamp(float value) {
    proto_.set_last_decay_timestamp(value);
  }
  ZeroStateHourBinPredictorProto proto_;
  ZeroStateHourBinPredictorConfig config_;
  DISALLOW_COPY_AND_ASSIGN(ZeroStateHourBinPredictor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
