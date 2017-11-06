// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/video_decode_stats_db_impl.h"

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task_scheduler/post_task.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "media/mojo/services/video_decode_stats.pb.h"

namespace media {

namespace {

// Avoid changing client name. Used in UMA.
// See comments in components/leveldb_proto/leveldb_database.h
const char kDatabaseClientName[] = "VideoDecodeStatsDB";

// Serialize the |entry| to a string to use as a key in the database.
std::string SerializeKey(const VideoDecodeStatsDB::VideoDescKey& key) {
  return base::StringPrintf("%d|%s|%d", static_cast<int>(key.codec_profile),
                            key.size.ToString().c_str(), key.frame_rate);
}

// For debug logging.
std::string KeyToString(const VideoDecodeStatsDB::VideoDescKey& key) {
  return "Key {" + SerializeKey(key) + "}";
}

// For debug logging.
std::string EntryToString(const VideoDecodeStatsDB::DecodeStatsEntry& entry) {
  return base::StringPrintf("DecodeStatsEntry {frames decoded:%" PRIu64
                            ", dropped:%" PRIu64
                            ", power efficient decoded:%" PRIu64 "}",
                            entry.frames_decoded, entry.frames_dropped,
                            entry.frames_decoded_power_efficient);
}

};  // namespace

VideoDecodeStatsDBImplFactory::VideoDecodeStatsDBImplFactory(
    base::FilePath db_dir)
    : db_dir_(db_dir) {
  DVLOG(2) << __func__ << " db_dir:" << db_dir_;
}

VideoDecodeStatsDBImplFactory::~VideoDecodeStatsDBImplFactory() = default;

std::unique_ptr<VideoDecodeStatsDB> VideoDecodeStatsDBImplFactory::CreateDB() {
  std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db_;

  auto inner_db =
      std::make_unique<leveldb_proto::ProtoDatabaseImpl<DecodeStatsProto>>(
          base::CreateSequencedTaskRunnerWithTraits(
              {base::MayBlock(), base::TaskPriority::BACKGROUND,
               base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));

  // Temporarily disallow writing to the DB until we've enabled clearing via
  // chrome://settings/clearBrowserData
  bool allow_writes = false;

  return std::make_unique<VideoDecodeStatsDBImpl>(std::move(inner_db), db_dir_,
                                                  allow_writes);
}

VideoDecodeStatsDBImpl::VideoDecodeStatsDBImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db,
    const base::FilePath& db_dir,
    bool allow_writes)
    : db_(std::move(db)),
      db_dir_(db_dir),
      allow_writes_(allow_writes),
      weak_ptr_factory_(this) {
  DCHECK(db_);
  DCHECK(!db_dir_.empty());
}

VideoDecodeStatsDBImpl::~VideoDecodeStatsDBImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodeStatsDBImpl::Initialize(
    base::OnceCallback<void(bool)> init_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(init_cb);
  // "Simple options" will use the default global cache of 8MB. In the worst
  // case our whole DB will be less than 35K, so we aren't worried about
  // spamming the cache.
  // TODO(chcunningham): Keep an eye on the size as the table evolves.
  db_->Init(kDatabaseClientName, db_dir_, leveldb_proto::CreateSimpleOptions(),
            base::BindOnce(&VideoDecodeStatsDBImpl::OnInit,
                           weak_ptr_factory_.GetWeakPtr(), std::move(init_cb)));
}

void VideoDecodeStatsDBImpl::OnInit(base::OnceCallback<void(bool)> init_cb,
                                    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");

  db_init_ = true;

  // Can't use DB when initialization fails.
  // TODO(chcunningham): Record UMA.
  if (!success)
    db_.reset();

  std::move(init_cb).Run(success);
}

bool VideoDecodeStatsDBImpl::IsInitialized() {
  // |db_| will be null if Initialization failed.
  return db_init_ && db_;
}

void VideoDecodeStatsDBImpl::AppendDecodeStats(const VideoDescKey& key,
                                               const DecodeStatsEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  DVLOG(3) << __func__ << " Reading key " << KeyToString(key)
           << " from DB with intent to update with " << EntryToString(entry);

  db_->GetEntry(SerializeKey(key),
                base::BindOnce(&VideoDecodeStatsDBImpl::WriteUpdatedEntry,
                               weak_ptr_factory_.GetWeakPtr(), key, entry));
}

void VideoDecodeStatsDBImpl::GetDecodeStats(const VideoDescKey& key,
                                            GetDecodeStatsCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  db_->GetEntry(
      SerializeKey(key),
      base::BindOnce(&VideoDecodeStatsDBImpl::OnGotDecodeStats,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void VideoDecodeStatsDBImpl::WriteUpdatedEntry(
    const VideoDescKey& key,
    const DecodeStatsEntry& entry,
    bool read_success,
    std::unique_ptr<DecodeStatsProto> prev_stats_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsInitialized());

  // TODO(chcunningham): Always allow writes once the DB can be cleared.
  if (!allow_writes_) {
    DVLOG(3) << __func__ << " IGNORING WRITE - writes not allowed";
    return;
  }

  // TODO(chcunningham): Record UMA.
  if (!read_success) {
    DVLOG(2) << __func__ << " FAILED DB read for " << KeyToString(key)
             << "; ignoring update!";
    return;
  }

  if (!prev_stats_proto) {
    prev_stats_proto.reset(new DecodeStatsProto());
    prev_stats_proto->set_frames_decoded(0);
    prev_stats_proto->set_frames_dropped(0);
    prev_stats_proto->set_frames_decoded_power_efficient(0);
  }

  uint64_t sum_frames_decoded =
      prev_stats_proto->frames_decoded() + entry.frames_decoded;
  uint64_t sum_frames_dropped =
      prev_stats_proto->frames_dropped() + entry.frames_dropped;
  uint64_t sum_frames_decoded_power_efficient =
      prev_stats_proto->frames_decoded_power_efficient() +
      entry.frames_decoded_power_efficient;

  prev_stats_proto->set_frames_decoded(sum_frames_decoded);
  prev_stats_proto->set_frames_dropped(sum_frames_dropped);
  prev_stats_proto->set_frames_decoded_power_efficient(
      sum_frames_decoded_power_efficient);

  DVLOG(3) << __func__ << " Updating " << KeyToString(key) << " with "
           << EntryToString(entry) << " aggregate:"
           << EntryToString(
                  DecodeStatsEntry(sum_frames_decoded, sum_frames_dropped,
                                   sum_frames_decoded_power_efficient));

  using ProtoDecodeStatsEntry = leveldb_proto::ProtoDatabase<DecodeStatsProto>;
  std::unique_ptr<ProtoDecodeStatsEntry::KeyEntryVector> entries =
      std::make_unique<ProtoDecodeStatsEntry::KeyEntryVector>();

  entries->emplace_back(SerializeKey(key), *prev_stats_proto);

  db_->UpdateEntries(std::move(entries),
                     std::make_unique<leveldb_proto::KeyVector>(),
                     base::BindOnce(&VideoDecodeStatsDBImpl::OnEntryUpdated,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecodeStatsDBImpl::OnEntryUpdated(bool success) {
  // TODO(chcunningham): record UMA.
  DVLOG(3) << __func__ << " update " << (success ? "succeeded" : "FAILED!");
}

void VideoDecodeStatsDBImpl::OnGotDecodeStats(
    GetDecodeStatsCB callback,
    bool success,
    std::unique_ptr<DecodeStatsProto> stats_proto) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(chcunningham): record UMA.

  std::unique_ptr<DecodeStatsEntry> entry;
  if (stats_proto) {
    DCHECK(success);
    entry = std::make_unique<DecodeStatsEntry>(
        stats_proto->frames_decoded(), stats_proto->frames_dropped(),
        stats_proto->frames_decoded_power_efficient());
  }

  DVLOG(3) << __func__ << " read " << (success ? "succeeded" : "FAILED!")
           << " entry: " << (entry ? EntryToString(*entry) : "nullptr");

  std::move(callback).Run(success, std::move(entry));
}

}  // namespace media
