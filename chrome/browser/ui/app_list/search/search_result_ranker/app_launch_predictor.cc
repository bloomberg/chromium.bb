// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"

#include <cmath>

#include "base/logging.h"
#include "base/stl_util.h"

namespace app_list {
namespace {

constexpr int kHoursADay = 24;
constexpr base::TimeDelta kSaveInternal = base::TimeDelta::FromHours(1);

// A bin with index i has 5 adjacent bins as: i + 0, i + 1, i + 2, i + 22, and
// i + 23. They each contributes to the final Rank score with different level:
// 0.6 for i-th bin itself, 0.15 for i + 1 (one hour later) and i + 23 (
// one hour earlier), 0.05 for i + 2 (two hours later) and i + 22 (two hours
// earlier).
constexpr int kAdjacentHourBin[] = {0, 1, 2, 22, 23};
constexpr float kAdjacentHourWeight[] = {0.6, 0.15, 0.05, 0.05, 0.15};

}  // namespace

MrfuAppLaunchPredictor::MrfuAppLaunchPredictor() = default;
MrfuAppLaunchPredictor::~MrfuAppLaunchPredictor() = default;

void MrfuAppLaunchPredictor::Train(const std::string& app_id) {
  // Updates the score for this |app_id|.
  ++num_of_trains_;
  Score& score = scores_[app_id];
  UpdateScore(&score);
  score.last_score += 1.0f - decay_coeff_;
}

base::flat_map<std::string, float> MrfuAppLaunchPredictor::Rank() {
  // Updates all scores and return app_id to score map.
  base::flat_map<std::string, float> output;
  for (auto& pair : scores_) {
    UpdateScore(&pair.second);
    output[pair.first] = pair.second.last_score;
  }
  return output;
}

const char MrfuAppLaunchPredictor::kPredictorName[] = "MrfuAppLaunchPredictor";
const char* MrfuAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool MrfuAppLaunchPredictor::ShouldSave() {
  // MrfuAppLaunchPredictor doesn't need materialization.
  return false;
}

AppLaunchPredictorProto MrfuAppLaunchPredictor::ToProto() const {
  // MrfuAppLaunchPredictor doesn't need materialization.
  NOTREACHED();
  return AppLaunchPredictorProto();
}

bool MrfuAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  // MrfuAppLaunchPredictor doesn't need materialization.
  NOTREACHED();
  return false;
}

void MrfuAppLaunchPredictor::UpdateScore(Score* score) {
  // Updates last_score and num_of_trains_at_last_update.
  const int trains_since_last_time =
      num_of_trains_ - score->num_of_trains_at_last_update;
  if (trains_since_last_time > 0) {
    score->last_score *= std::pow(decay_coeff_, trains_since_last_time);
    score->num_of_trains_at_last_update = num_of_trains_;
  }
}

SerializedMrfuAppLaunchPredictor::SerializedMrfuAppLaunchPredictor()
    : MrfuAppLaunchPredictor(), last_save_timestamp_(base::Time::Now()) {}

SerializedMrfuAppLaunchPredictor::~SerializedMrfuAppLaunchPredictor() = default;

const char SerializedMrfuAppLaunchPredictor::kPredictorName[] =
    "SerializedMrfuAppLaunchPredictor";
const char* SerializedMrfuAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool SerializedMrfuAppLaunchPredictor::ShouldSave() {
  const base::Time now = base::Time::Now();
  if (now - last_save_timestamp_ >= kSaveInternal) {
    last_save_timestamp_ = now;
    return true;
  }
  return false;
}

AppLaunchPredictorProto SerializedMrfuAppLaunchPredictor::ToProto() const {
  AppLaunchPredictorProto output;
  auto& predictor_proto =
      *output.mutable_serialized_mrfu_app_launch_predictor();
  predictor_proto.set_num_of_trains(num_of_trains_);
  for (const auto& pair : scores_) {
    auto& score_item = (*predictor_proto.mutable_scores())[pair.first];
    score_item.set_last_score(pair.second.last_score);
    score_item.set_num_of_trains_at_last_update(
        pair.second.num_of_trains_at_last_update);
  }
  return output;
}

bool SerializedMrfuAppLaunchPredictor::FromProto(
    const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kSerializedMrfuAppLaunchPredictor) {
    return false;
  }

  const auto& predictor_proto = proto.serialized_mrfu_app_launch_predictor();
  num_of_trains_ = predictor_proto.num_of_trains();

  scores_.clear();
  for (const auto& pair : predictor_proto.scores()) {
    // Skip the case where the last_score has already dropped to 0.0f.
    if (pair.second.last_score() == 0.0f)
      continue;
    auto& score_item = scores_[pair.first];
    score_item.last_score = pair.second.last_score();
    score_item.num_of_trains_at_last_update =
        pair.second.num_of_trains_at_last_update();
  }

  return true;
}

HourAppLaunchPredictor::HourAppLaunchPredictor()
    : last_save_timestamp_(base::Time::Now()) {}

HourAppLaunchPredictor::~HourAppLaunchPredictor() = default;

void HourAppLaunchPredictor::Train(const std::string& app_id) {
  auto& frequency_table = (*proto_.mutable_hour_app_launch_predictor()
                                ->mutable_binned_frequency_table())[GetBin()];

  frequency_table.set_total_counts(frequency_table.total_counts() + 1);
  (*frequency_table.mutable_frequency())[app_id] += 1;
}

base::flat_map<std::string, float> HourAppLaunchPredictor::Rank() {
  base::flat_map<std::string, float> output;
  const int bin = GetBin();
  const bool is_weekend = bin >= kHoursADay;
  const int hour = bin % kHoursADay;
  const auto& frequency_table_map =
      proto_.hour_app_launch_predictor().binned_frequency_table();

  for (size_t i = 0; i < base::size(kAdjacentHourBin); ++i) {
    // Finds adjacent bin and weight.
    const int adj_bin =
        (hour + kAdjacentHourBin[i]) % kHoursADay + kHoursADay * is_weekend;
    const auto find_frequency_table = frequency_table_map.find(adj_bin);
    if (find_frequency_table == frequency_table_map.end())
      continue;

    const auto& frequency_table = find_frequency_table->second;
    const float weight = kAdjacentHourWeight[i];

    // Accumulates the frequency to the output.
    if (frequency_table.total_counts() > 0) {
      const int total_counts = frequency_table.total_counts();
      for (const auto& pair : frequency_table.frequency()) {
        output[pair.first] +=
            static_cast<float>(pair.second) / total_counts * weight;
      }
    }
  }
  return output;
}

const char HourAppLaunchPredictor::kPredictorName[] = "HourAppLaunchPredictor";
const char* HourAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool HourAppLaunchPredictor::ShouldSave() {
  const base::Time now = base::Time::Now();
  if (now - last_save_timestamp_ >= kSaveInternal) {
    last_save_timestamp_ = now;
    return true;
  }
  return false;
}

AppLaunchPredictorProto HourAppLaunchPredictor::ToProto() const {
  return proto_;
}

bool HourAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kHourAppLaunchPredictor) {
    return false;
  }
  proto_ = proto;
  return true;
}

int HourAppLaunchPredictor::GetBin() const {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  const bool is_weekend = now.day_of_week == 6 || now.day_of_week == 0;

  // To distinguish workdays with weekends, we use now.hour for workdays, and
  // now.hour + 24 for weekends.
  if (!is_weekend) {
    return now.hour;
  } else {
    return now.hour + kHoursADay;
  }
}

void FakeAppLaunchPredictor::SetShouldSave(bool should_save) {
  should_save_ = should_save;
}

void FakeAppLaunchPredictor::Train(const std::string& app_id) {
  // Increases 1.0 for rank score of app_id.
  (*proto_.mutable_fake_app_launch_predictor()
        ->mutable_rank_result())[app_id] += 1.0f;
}

base::flat_map<std::string, float> FakeAppLaunchPredictor::Rank() {
  // Outputs proto_.fake_app_launch_predictor().rank_result() as Rank result.
  base::flat_map<std::string, float> output;
  for (const auto& pair : proto_.fake_app_launch_predictor().rank_result()) {
    output[pair.first] = pair.second;
  }
  return output;
}

const char FakeAppLaunchPredictor::kPredictorName[] = "FakeAppLaunchPredictor";
const char* FakeAppLaunchPredictor::GetPredictorName() const {
  return kPredictorName;
}

bool FakeAppLaunchPredictor::ShouldSave() {
  return should_save_;
}

AppLaunchPredictorProto FakeAppLaunchPredictor::ToProto() const {
  return proto_;
}

bool FakeAppLaunchPredictor::FromProto(const AppLaunchPredictorProto& proto) {
  if (proto.predictor_case() !=
      AppLaunchPredictorProto::kFakeAppLaunchPredictor) {
    return false;
  }
  proto_ = proto;
  return true;
}

}  // namespace app_list
