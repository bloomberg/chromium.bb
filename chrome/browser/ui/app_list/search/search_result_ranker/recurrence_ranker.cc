// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"

namespace app_list {
namespace {

using base::Optional;
using base::Time;
using base::TimeDelta;

void SaveProtoToDisk(const base::FilePath& filepath,
                     const RecurrenceRankerProto& proto) {
  std::string proto_str;
  if (!proto.SerializeToString(&proto_str)) {
    LogSerializationError(SerializationError::kToProtoError);
    return;
  }

  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "RecurrenceRanker");
  }
  if (!write_result) {
    LogSerializationError(SerializationError::kModelWriteError);
  }
}

// Try to load a |RecurrenceRankerProto| from the given filepath. If it fails,
// it returns nullptr.
std::unique_ptr<RecurrenceRankerProto> LoadProtoFromDisk(
    const base::FilePath& filepath) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    LogSerializationError(SerializationError::kModelReadError);
    return nullptr;
  }

  auto proto = std::make_unique<RecurrenceRankerProto>();
  if (!proto->ParseFromString(proto_str)) {
    LogSerializationError(SerializationError::kFromProtoError);
    return nullptr;
  }
  return proto;
}

// Returns a new, configured instance of the predictor defined in |config|.
std::unique_ptr<RecurrencePredictor> MakePredictor(
    const RecurrenceRankerConfigProto& config) {
  if (config.has_fake_predictor())
    return std::make_unique<FakePredictor>(config.fake_predictor());
  if (config.has_default_predictor())
    return std::make_unique<DefaultPredictor>(config.default_predictor());
  if (config.has_zero_state_frecency_predictor())
    return std::make_unique<ZeroStateFrecencyPredictor>(
        config.zero_state_frecency_predictor());
  if (config.has_zero_state_hour_bin_predictor())
    return std::make_unique<ZeroStateHourBinPredictor>(
        config.zero_state_hour_bin_predictor());

  LogConfigurationError(ConfigurationError::kInvalidPredictor);
  NOTREACHED();
  return nullptr;
}

std::vector<std::pair<std::string, float>> SortAndTruncateRanks(
    int n,
    const base::flat_map<std::string, float>& ranks) {
  std::vector<std::pair<std::string, float>> sorted_ranks(ranks.begin(),
                                                          ranks.end());
  std::sort(sorted_ranks.begin(), sorted_ranks.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });

  // vector::resize simply truncates the array if there are more than n
  // elements. Note this is still O(N).
  if (sorted_ranks.size() > static_cast<size_t>(n))
    sorted_ranks.resize(n);
  return sorted_ranks;
}

base::flat_map<std::string, float> ZipTargetsWithScores(
    const base::flat_map<std::string, FrecencyStore::ValueData>& target_to_id,
    const base::flat_map<unsigned int, float>& id_to_score) {
  base::flat_map<std::string, float> target_to_score;
  for (const auto& pair : target_to_id) {
    DCHECK(pair.second.last_num_updates ==
           target_to_id.begin()->second.last_num_updates);
    const auto& it = id_to_score.find(pair.second.id);
    if (it != id_to_score.end()) {
      target_to_score[pair.first] = it->second;
    }
  }

  return target_to_score;
}

base::flat_map<std::string, float> GetScoresFromFrecencyStore(
    const base::flat_map<std::string, FrecencyStore::ValueData>& target_to_id) {
  base::flat_map<std::string, float> target_to_score;
  for (const auto& pair : target_to_id) {
    DCHECK(pair.second.last_num_updates ==
           target_to_id.begin()->second.last_num_updates);
    target_to_score[pair.first] = pair.second.last_score;
  }
  return target_to_score;
}

}  // namespace

RecurrenceRanker::RecurrenceRanker(const base::FilePath& filepath,
                                   const RecurrenceRankerConfigProto& config,
                                   bool is_ephemeral_user)
    : proto_filepath_(filepath),
      config_hash_(base::PersistentHash(config.SerializeAsString())),
      is_ephemeral_user_(is_ephemeral_user),
      min_seconds_between_saves_(
          TimeDelta::FromSeconds(config.min_seconds_between_saves())),
      time_of_last_save_(Time::Now()),
      weak_factory_(this) {
  task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  targets_ = std::make_unique<FrecencyStore>(config.target_limit(),
                                             config.target_decay());
  conditions_ = std::make_unique<FrecencyStore>(config.condition_limit(),
                                                config.condition_decay());

  if (is_ephemeral_user_) {
    // Ephemeral users have no persistent storage, so we don't try and load the
    // proto from disk. Instead, we fall back on using a default (frecency)
    // predictor, which is still useful with only data from the current session.
    predictor_ = std::make_unique<DefaultPredictor>(config.default_predictor());
  } else {
    predictor_ = MakePredictor(config);

    // Load the proto from disk and finish initialisation in
    // |OnLoadProtoFromDiskComplete|.
    base::PostTaskAndReplyWithResult(
        task_runner_.get(), FROM_HERE,
        base::BindOnce(&LoadProtoFromDisk, proto_filepath_),
        base::BindOnce(&RecurrenceRanker::OnLoadProtoFromDiskComplete,
                       weak_factory_.GetWeakPtr()));
  }
}

RecurrenceRanker::~RecurrenceRanker() = default;

void RecurrenceRanker::OnLoadProtoFromDiskComplete(
    std::unique_ptr<RecurrenceRankerProto> proto) {
  load_from_disk_completed_ = true;

  // If OnLoadFromDisk returned nullptr, no saved ranker proto was available on
  // disk, and there is nothing to load.
  if (!proto) {
    return;
  }

  if (!proto->has_config_hash() || proto->config_hash() != config_hash_) {
    // The configuration of the saved ranker doesn't match the configuration for
    // this object. We should not use any data from it, and instead start with a
    // clean slate. This is not always an error: it is expected if, for example,
    // a RecurrenceRanker instance is rolled out in one release, and then
    // reconfigured in the next.
    LogConfigurationError(ConfigurationError::kHashMismatch);
    return;
  }

  if (proto->has_predictor())
    predictor_->FromProto(proto->predictor());
  else
    LogSerializationError(SerializationError::kPredictorMissingError);

  if (proto->has_targets())
    targets_->FromProto(proto->targets());
  else
    LogSerializationError(SerializationError::kTargetsMissingError);

  if (proto->has_conditions())
    conditions_->FromProto(proto->conditions());
  else
    LogSerializationError(SerializationError::kConditionsMissingError);
}

void RecurrenceRanker::Record(const std::string& target) {
  if (!load_from_disk_completed_)
    return;

  if (predictor_->GetPredictorName() == DefaultPredictor::kPredictorName) {
    targets_->Update(target);
  } else {
    predictor_->Train(targets_->Update(target));
  }

  MaybeSave();
}

void RecurrenceRanker::Record(const std::string& target,
                              const std::string& condition) {
  if (!load_from_disk_completed_)
    return;

  if (predictor_->GetPredictorName() == DefaultPredictor::kPredictorName) {
    // TODO(921444): The default predictor does not support queries, so we fail
    // here. Once we have a suitable query-based default predictor implemented,
    // change this.
    LogUsageError(UsageError::kInvalidTrainCall);
    NOTREACHED();
  } else {
    predictor_->Train(targets_->Update(target), conditions_->Update(condition));
  }

  MaybeSave();
}

void RecurrenceRanker::RenameTarget(const std::string& target,
                                    const std::string& new_target) {
  if (!load_from_disk_completed_)
    return;

  targets_->Rename(target, new_target);
  MaybeSave();
}

void RecurrenceRanker::RemoveTarget(const std::string& target) {
  // TODO(tby): Find a solution to the edge case of a removal before disk
  // loading is complete, resulting in the remove getting dropped.
  if (!load_from_disk_completed_)
    return;

  targets_->Remove(target);
  MaybeSave();
}

void RecurrenceRanker::RenameCondition(const std::string& condition,
                                       const std::string& new_condition) {
  if (!load_from_disk_completed_)
    return;

  conditions_->Rename(condition, new_condition);
  MaybeSave();
}

void RecurrenceRanker::RemoveCondition(const std::string& condition) {
  if (!load_from_disk_completed_)
    return;

  conditions_->Remove(condition);
  MaybeSave();
}

base::flat_map<std::string, float> RecurrenceRanker::Rank() {
  if (!load_from_disk_completed_)
    return {};

  if (predictor_->GetPredictorName() == DefaultPredictor::kPredictorName)
    return GetScoresFromFrecencyStore(targets_->GetAll());
  return ZipTargetsWithScores(targets_->GetAll(), predictor_->Rank());
}

base::flat_map<std::string, float> RecurrenceRanker::Rank(
    const std::string& condition) {
  if (!load_from_disk_completed_)
    return {};

  base::Optional<unsigned int> condition_id = conditions_->GetId(condition);
  if (condition_id == base::nullopt)
    return {};

  if (predictor_->GetPredictorName() == DefaultPredictor::kPredictorName) {
    // TODO(921444): The default predictor does not support queries, so we fail
    // here. Once we have a suitable query-based default predictor implemented,
    // change this.
    LogUsageError(UsageError::kInvalidRankCall);
    NOTREACHED();
    return {};
  }
  return ZipTargetsWithScores(targets_->GetAll(),
                              predictor_->Rank(condition_id.value()));
}

std::vector<std::pair<std::string, float>> RecurrenceRanker::RankTopN(int n) {
  if (!load_from_disk_completed_)
    return {};

  return SortAndTruncateRanks(n, Rank());
}

std::vector<std::pair<std::string, float>> RecurrenceRanker::RankTopN(
    int n,
    const std::string& condition) {
  if (!load_from_disk_completed_)
    return {};

  return SortAndTruncateRanks(n, Rank(condition));
}

void RecurrenceRanker::MaybeSave() {
  if (is_ephemeral_user_)
    return;

  if (Time::Now() - time_of_last_save_ > min_seconds_between_saves_) {
    time_of_last_save_ = Time::Now();
    RecurrenceRankerProto proto;
    ToProto(&proto);
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SaveProtoToDisk, proto_filepath_, proto));
  }
}

void RecurrenceRanker::ToProto(RecurrenceRankerProto* proto) {
  proto->set_config_hash(config_hash_);
  predictor_->ToProto(proto->mutable_predictor());
  targets_->ToProto(proto->mutable_targets());
  conditions_->ToProto(proto->mutable_conditions());
}

void RecurrenceRanker::ForceSaveOnNextUpdateForTesting() {
  time_of_last_save_ = Time::UnixEpoch();
}

const char* RecurrenceRanker::GetPredictorNameForTesting() const {
  return predictor_->GetPredictorName();
}

}  // namespace app_list
