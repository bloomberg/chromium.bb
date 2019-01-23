// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <cmath>

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"

namespace app_list {

FakePredictor::FakePredictor() = default;
FakePredictor::~FakePredictor() = default;

void FakePredictor::Train(unsigned int target) {
  counts_[target] += 1.0f;
}

base::flat_map<unsigned int, float> FakePredictor::Rank() {
  return counts_;
}

const char FakePredictor::kPredictorName[] = "FakePredictor";
const char* FakePredictor::GetPredictorName() const {
  return kPredictorName;
}

void FakePredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* counts = proto->mutable_fake_predictor()->mutable_counts();
  for (auto& pair : counts_)
    (*counts)[pair.first] = pair.second;
}

void FakePredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_fake_predictor())
    return;

  auto fake_predictor_proto = proto.fake_predictor();
  for (const auto& pair : fake_predictor_proto.counts()) {
    counts_[pair.first] = pair.second;
  }
}

// |FrecencyPredictor| uses the |RecurrenceRanker|'s store of targets for
// ranking, which is updated on calls to |RecurrenceRanker::Record|. So, there
// is no training work for the predictor itself to do.
void FrecencyPredictor::Train(unsigned int target) {}

// |FrecencyPredictor::Rank| is special-cased inside |RecurrenceRanker::Record|
// for efficiency reasons, so as not to do uneccessary conversion of targets to
// ids and back.
base::flat_map<unsigned int, float> FrecencyPredictor::Rank() {
  NOTREACHED();
  return {};
}

const char FrecencyPredictor::kPredictorName[] = "FrecencyPredictor";
const char* FrecencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

// Empty as all data used by the frecency predictor is serialised with
// |RecurrenceRanker|.
void FrecencyPredictor::ToProto(RecurrencePredictorProto* proto) const {}

// Empty as all data used by the frecency predictor is serialised with
// |RecurrenceRanker|.
void FrecencyPredictor::FromProto(const RecurrencePredictorProto& proto) {}

}  // namespace app_list
