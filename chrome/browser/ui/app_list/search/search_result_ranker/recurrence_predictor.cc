// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"

#include <cmath>

#include "base/logging.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"

namespace app_list {

FakePredictor::FakePredictor(FakePredictorConfig config) {}
FakePredictor::~FakePredictor() = default;

const char FakePredictor::kPredictorName[] = "FakePredictor";
const char* FakePredictor::GetPredictorName() const {
  return kPredictorName;
}

void FakePredictor::Train(const std::string& target, const std::string& query) {
  counts_[target] += 1.0f;
}

base::flat_map<std::string, float> FakePredictor::Rank(
    const std::string& query) {
  return counts_;
}

void FakePredictor::Rename(const std::string& target,
                           const std::string& new_target) {
  auto it = counts_.find(target);
  if (it != counts_.end()) {
    counts_[new_target] = it->second;
    counts_.erase(it);
  }
}

void FakePredictor::Remove(const std::string& target) {
  auto it = counts_.find(target);
  if (it != counts_.end())
    counts_.erase(it);
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

ZeroStateFrecencyPredictor::ZeroStateFrecencyPredictor(
    ZeroStateFrecencyPredictorConfig config)
    : targets_(std::make_unique<FrecencyStore>(config.target_limit(),
                                               config.decay_coeff())) {}
ZeroStateFrecencyPredictor::~ZeroStateFrecencyPredictor() = default;

const char ZeroStateFrecencyPredictor::kPredictorName[] =
    "ZeroStateFrecencyPredictor";
const char* ZeroStateFrecencyPredictor::GetPredictorName() const {
  return kPredictorName;
}

void ZeroStateFrecencyPredictor::Train(const std::string& target,
                                       const std::string& query) {
  if (!query.empty()) {
    NOTREACHED();
    LOG(ERROR) << "ZeroStateFrecencyPredictor was passed a query.";
    return;
  }

  targets_->Update(target);
}

base::flat_map<std::string, float> ZeroStateFrecencyPredictor::Rank(
    const std::string& query) {
  if (!query.empty()) {
    NOTREACHED();
    LOG(ERROR) << "ZeroStateFrecencyPredictor was passed a query.";
    return {};
  }

  base::flat_map<std::string, float> ranks;
  for (const auto& target : targets_->GetAll())
    ranks[target.first] = target.second.last_score;
  return ranks;
}

void ZeroStateFrecencyPredictor::Rename(const std::string& target,
                                        const std::string& new_target) {
  targets_->Rename(target, new_target);
}

void ZeroStateFrecencyPredictor::Remove(const std::string& target) {
  targets_->Remove(target);
}

void ZeroStateFrecencyPredictor::ToProto(
    RecurrencePredictorProto* proto) const {
  auto* targets =
      proto->mutable_zero_state_frecency_predictor()->mutable_targets();
  targets_->ToProto(targets);
}

void ZeroStateFrecencyPredictor::FromProto(
    const RecurrencePredictorProto& proto) {
  if (!proto.has_zero_state_frecency_predictor())
    return;

  const auto& predictor = proto.zero_state_frecency_predictor();
  if (predictor.has_targets())
    targets_->FromProto(predictor.targets());
}

}  // namespace app_list
