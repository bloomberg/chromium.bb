// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {

using FakePredictorConfig = RecurrenceRankerConfigProto::FakePredictorConfig;
using ZeroStateFrecencyPredictorConfig =
    RecurrenceRankerConfigProto::ZeroStateFrecencyPredictorConfig;

class FrecencyStore;

// |RecurrencePredictor| is the interface for all predictors used by
// |RecurrenceRanker| to drive rankings. If a predictor has some form of
// serialisation, it should have a corresponding proto in
// |recurrence_predictor.proto|.
class RecurrencePredictor {
 public:
  virtual ~RecurrencePredictor() = default;

  // Train the predictor on an occurrence of |target| coinciding with |query|.
  // The predictor will collect its own contextual information, eg. time of day,
  // as part of training. Zero-state scenarios should use an empty string for
  // |query|.
  virtual void Train(const std::string& target, const std::string& query) = 0;
  // Return a map of all known targets to their scores for the given query
  // under this predictor. Scores must be within the range [0,1]. Zero-state
  // scenarios should use an empty string for |query|.
  virtual base::flat_map<std::string, float> Rank(const std::string& query) = 0;

  // Rename a target, while keeping learned information on it.
  virtual void Rename(const std::string& target,
                      const std::string& new_target) = 0;
  // Remove a target entirely.
  virtual void Remove(const std::string& target) = 0;

  virtual void ToProto(RecurrencePredictorProto* proto) const = 0;
  virtual void FromProto(const RecurrencePredictorProto& proto) = 0;
  virtual const char* GetPredictorName() const = 0;
};

// |FakePredictor| is a simple 'predictor' used for testing. |Rank| returns the
// numbers of times each target has been trained on, and ignores the query
// altogether.
//
// WARNING: this breaks the guarantees on the range of values a score can take,
// so should not be used for anything except testing.
class FakePredictor : public RecurrencePredictor {
 public:
  explicit FakePredictor(FakePredictorConfig config);
  ~FakePredictor() override;

  // RecurrencePredictor:
  void Train(const std::string& target, const std::string& query) override;
  base::flat_map<std::string, float> Rank(const std::string& query) override;
  void Rename(const std::string& target,
              const std::string& new_target) override;
  void Remove(const std::string& target) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  base::flat_map<std::string, float> counts_;

  DISALLOW_COPY_AND_ASSIGN(FakePredictor);
};

// |ZeroStateFrecencyPredictor| ranks targets according to their frecency, and
// can only be used for zero-state predictions, that is, an empty query string.
class ZeroStateFrecencyPredictor : public RecurrencePredictor {
 public:
  explicit ZeroStateFrecencyPredictor(ZeroStateFrecencyPredictorConfig config);
  ~ZeroStateFrecencyPredictor() override;

  // RecurrencePredictor:
  void Train(const std::string& target, const std::string& query) override;
  base::flat_map<std::string, float> Rank(const std::string& query) override;
  void Rename(const std::string& target,
              const std::string& new_target) override;
  void Remove(const std::string& target) override;
  void ToProto(RecurrencePredictorProto* proto) const override;
  void FromProto(const RecurrencePredictorProto& proto) override;
  const char* GetPredictorName() const override;

  static const char kPredictorName[];

 private:
  std::unique_ptr<FrecencyStore> targets_;

  DISALLOW_COPY_AND_ASSIGN(ZeroStateFrecencyPredictor);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_RESULT_RANKER_RECURRENCE_PREDICTOR_H_
