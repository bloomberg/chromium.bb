// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <algorithm>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.pb.h"
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
    LOG(ERROR) << "Unable to serialize RecurrenceRankerProto.";
    return;
  }

  bool write_result;
  {
    base::ScopedBlockingCall scoped_blocking_call(
        base::BlockingType::MAY_BLOCK);
    write_result = base::ImportantFileWriter::WriteFileAtomically(
        filepath, proto_str, "RecurrenceRanker");
  }
  if (!write_result)
    LOG(ERROR) << "Error writing ranker file " << filepath;
}

// Try to load a |RecurrenceRankerProto| from the given filepath. If it fails,
// it returns an empty default instance of |RecurrenceRankerProto|. Guaranteed
// to be non-null.
std::unique_ptr<RecurrenceRankerProto> LoadProtoFromDisk(
    const base::FilePath& filepath) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  std::string proto_str;
  if (!base::ReadFileToString(filepath, &proto_str)) {
    LOG(ERROR) << "Error reading ranker file " << filepath;
    return std::make_unique<RecurrenceRankerProto>();
  }

  auto proto = std::make_unique<RecurrenceRankerProto>();
  if (!proto->ParseFromString(proto_str)) {
    LOG(ERROR) << "Error parsing ranker file " << filepath;
    return std::make_unique<RecurrenceRankerProto>();
  }
  return proto;
}

// Returns a new, configured instance of the predictor defined in |config|.
std::unique_ptr<RecurrencePredictor> MakePredictor(
    RecurrenceRankerConfigProto config) {
  if (config.has_frecency_predictor())
    return std::make_unique<FrecencyPredictor>();
  if (config.has_fake_predictor())
    return std::make_unique<FakePredictor>();

  NOTREACHED();
  return nullptr;
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
                                             config.target_decay_coeff());
  if (is_ephemeral_user_) {
    // Ephemeral users have no persistent storage, so we don't try and load the
    // proto from disk. Instead, we fall back on using a frecency predictor,
    // which is still useful with only data from the current session.
    predictor_ = std::make_unique<FrecencyPredictor>();
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
  // The configuration of the saved ranker doesn't match the configuration for
  // this object. We should not use any data from it, and instead start with a
  // clean slate.
  if (!proto->has_config_hash() || proto->config_hash() != config_hash_) {
    load_from_disk_completed_ = true;
    return;
  }

  if (proto->has_targets())
    targets_->FromProto(proto->targets());
  if (proto->has_predictor())
    predictor_->FromProto(proto->predictor());

  load_from_disk_completed_ = true;
}

void RecurrenceRanker::Record(const std::string& target) {
  if (!load_from_disk_completed_)
    return;

  targets_->Update(target);

  // It might be possible that, despite just being updated, the target was
  // removed from the store. Only train if the target is still valid.
  Optional<unsigned int> id = targets_->GetId(target);
  if (id.has_value())
    predictor_->Train(id.value());
  MaybeSave();
}

void RecurrenceRanker::Rename(const std::string& target,
                              const std::string& new_target) {
  if (!load_from_disk_completed_)
    return;

  targets_->Rename(target, new_target);
  MaybeSave();
}

void RecurrenceRanker::Remove(const std::string& target) {
  if (!load_from_disk_completed_)
    return;

  targets_->Remove(target);
  MaybeSave();
}

base::flat_map<std::string, float> RecurrenceRanker::Rank() {
  if (!load_from_disk_completed_)
    return {};

  // Special case for a frecency predictor. Because this is simply a wrapper
  // around the |RecurrenceRanker|'s targets store, we can directly return the
  // contents of the store and avoid an uneccessary iteration through targets.
  if (predictor_->GetPredictorName() == FrecencyPredictor::kPredictorName) {
    base::flat_map<std::string, float> ranks;
    for (const auto& pair : targets_->GetAll())
      ranks[pair.first] = pair.second.last_score;
    return ranks;
  }

  const base::flat_map<unsigned int, float> id_ranks = predictor_->Rank();
  const base::flat_map<std::string, FrecencyStore::ValueData>& targets =
      targets_->GetAll();

  base::flat_map<std::string, float> ranks;
  for (const auto& pair : targets) {
    const auto& data = pair.second;
    const auto it = id_ranks.find(data.id);
    if (it == id_ranks.end())
      continue;
    ranks[pair.first] = it->second;
  }
  return ranks;
}

std::vector<std::pair<std::string, float>> RecurrenceRanker::RankTopN(int n) {
  if (!load_from_disk_completed_)
    return {};

  base::flat_map<std::string, float> ranks = Rank();
  std::vector<std::pair<std::string, float>> sorted_ranks(ranks.begin(),
                                                          ranks.end());
  std::sort(sorted_ranks.begin(), sorted_ranks.end(),
            [](const std::pair<std::string, float>& a,
               const std::pair<std::string, float>& b) {
              return a.second > b.second;
            });

  // vector::resize simply truncates the array if there are more than n
  // elements. Note this is still O(N).
  if (sorted_ranks.size() > static_cast<unsigned long>(n))
    sorted_ranks.resize(n);
  return sorted_ranks;
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
}

void RecurrenceRanker::ForceSaveOnNextUpdateForTesting() {
  time_of_last_save_ = Time::UnixEpoch();
}

const char* RecurrenceRanker::GetPredictorNameForTesting() const {
  return predictor_->GetPredictorName();
}

}  // namespace app_list
