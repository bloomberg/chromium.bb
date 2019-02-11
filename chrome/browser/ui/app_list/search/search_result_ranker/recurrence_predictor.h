// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"

namespace app_list {

// |RecurrencePredictor| is the interface for all predictors used by
// |RecurrenceRanker| to drive rankings. If a predictor has some form of
// serialisation, it should have a corresponding proto in
// |recurrence_predictor.proto|.
class RecurrencePredictor {
 public:
  virtual ~RecurrencePredictor() = default;

  // Train the predictor on an occurrence of |target|. The predictor will
  // collect its own contextual information, eg. time of day, as part of
  // training.
  virtual void Train(unsigned int target) = 0;
  // Return a map of all known targets to their scores under this predictor.
  // Scores must be within the range [0,1].
  virtual base::flat_map<unsigned int, float> Rank() = 0;

  virtual void ToProto(RecurrencePredictorProto* proto) const = 0;
  virtual void FromProto(const RecurrencePredictorProto& proto) = 0;
  virtual const char* GetPredictorName() const = 0;
};

// |FakePredictor| is a simple 'predictor' used for testing. |Rank| returns the
// numbers of times each target has been trained on.
//
// WARNING: this breaks the guarantees on the range of values a score can take,
// so should not be used for anything except testing.
class FakePredictor : public RecurrencePredictor {
 public:
  FakePredictor();
  ~FakePredictor() override;

  // RecurrencePredictor overrides:
  void Train(unsigned int target) override;
  base::flat_map<unsigned int, float> Rank() override;
  const char* GetPredictorName() const override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;

  static const char kPredictorName[];

 private:
  base::flat_map<unsigned int, float> counts_;

  DISALLOW_COPY_AND_ASSIGN(FakePredictor);
};

// |FrecencyPredictor| simply returns targets in frecency order. To do this, it
// piggybacks off the existing targets FrecencyStore used in |RecurrenceRanker|.
// For efficiency reasons the ranker itself has a special case that handles the
// logic of |FrecencyPredictor::Rank|. This leaves |FrecencyPredictor| as a
// mostly empty class.
class FrecencyPredictor : public RecurrencePredictor {
 public:
  FrecencyPredictor() = default;
  ~FrecencyPredictor() override = default;

  // RecurrencePredictor overrides:
  void Train(unsigned int target) override;
  base::flat_map<unsigned int, float> Rank() override;
  const char* GetPredictorName() const override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;

  static const char kPredictorName[];

 private:
  DISALLOW_COPY_AND_ASSIGN(FrecencyPredictor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
