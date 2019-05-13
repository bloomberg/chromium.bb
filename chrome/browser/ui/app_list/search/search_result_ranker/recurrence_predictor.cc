// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <cmath>

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"

namespace app_list {

void RecurrencePredictor::Train(unsigned int target) {
  NOTREACHED();
}

void RecurrencePredictor::Train(unsigned int target, unsigned int condition) {
  NOTREACHED();
}

base::flat_map<unsigned int, float> RecurrencePredictor::Rank() {
  NOTREACHED();
  return {};
}

base::flat_map<unsigned int, float> RecurrencePredictor::Rank(
    unsigned int condition) {
  NOTREACHED();
  return {};
}

FakePredictor::FakePredictor(FakePredictorConfig config) {}
FakePredictor::~FakePredictor() = default;

const char FakePredictor::kPredictorName[] = "FakePredictor";
const char* FakePredictor::GetPredictorName() const {
  return kPredictorName;
}

void FakePredictor::Train(unsigned int target) {
  counts_[target] += 1.0f;
}

base::flat_map<unsigned int, float> FakePredictor::Rank() {
  return counts_;
}

void FakePredictor::ToProto(RecurrencePredictorProto* proto) const {
  auto* counts = proto->mutable_fake_predictor()->mutable_counts();
  for (auto& pair : counts_)
    (*counts)[pair.first] = pair.second;
}

void FakePredictor::FromProto(const RecurrencePredictorProto& proto) {
  if (!proto.has_fake_predictor())
    return;
  auto predictor = proto.fake_predictor();

  for (const auto& pair : predictor.counts())
    counts_[pair.first] = pair.second;
}

DefaultPredictor::DefaultPredictor(const DefaultPredictorConfig& config) {}
DefaultPredictor::~DefaultPredictor() {}

const char DefaultPredictor::kPredictorName[] = "DefaultPredictor";
const char* DefaultPredictor::GetPredictorName() const {
  return kPredictorName;
}

void DefaultPredictor::ToProto(RecurrencePredictorProto* proto) const {}

void DefaultPredictor::FromProto(const RecurrencePredictorProto& proto) {}

ZeroStateFrecencyPredictor::ZeroStateFrecencyPredictor(
    ZeroStateFrecencyPredictorConfig config)
    : decay_coeff_(config.decay_coeff()) {}
ZeroStateFrecencyPredictor::~ZeroStateFrecencyPredictor() = default;

const char ZeroStateFrecencyPredictor::kPredictorName[] =
    "ZeroStateFrecencyPredictor";
const char* ZeroStateFrecencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void ZeroStateFrecencyPredictor::Train(unsigned int target) {
  ++num_updates_;
  TargetData& data = targets_[target];
  DecayScore(&data);
  data.last_score += 1.0f - decay_coeff_;
}

base::flat_map<unsigned int, float> ZeroStateFrecencyPredictor::Rank() {
  base::flat_map<unsigned int, float> result;
  for (auto& pair : targets_) {
    DecayScore(&pair.second);
    result[pair.first] = pair.second.last_score;
  }
  return result;
}

void ZeroStateFrecencyPredictor::ToProto(
    RecurrencePredictorProto* proto) const {
  auto* predictor = proto->mutable_zero_state_frecency_predictor();

  predictor->set_num_updates(num_updates_);

  for (const auto& pair : targets_) {
    auto* target_data = predictor->add_targets();
    target_data->set_id(pair.first);
    target_data->set_last_score(pair.second.last_score);
    target_data->set_last_num_updates(pair.second.last_num_updates);
  }
}

void ZeroStateFrecencyPredictor::FromProto(
    const RecurrencePredictorProto& proto) {
  if (!proto.has_zero_state_frecency_predictor())
    return;
  const auto& predictor = proto.zero_state_frecency_predictor();

  num_updates_ = predictor.num_updates();

  base::flat_map<unsigned int, TargetData> targets;
  for (const auto& target_data : predictor.targets()) {
    targets[target_data.id()] = {target_data.last_score(),
                                 target_data.last_num_updates()};
  }
  targets_.swap(targets);
}

void ZeroStateFrecencyPredictor::DecayScore(TargetData* data) {
  int time_since_update = num_updates_ - data->last_num_updates;

  if (time_since_update > 0) {
    data->last_score *= std::pow(decay_coeff_, time_since_update);
    data->last_num_updates = num_updates_;
  }
}

}  // namespace app_list
