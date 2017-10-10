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
                            ", dropped:%" PRIu64 "}",
                            entry.frames_decoded, entry.frames_dropped);
}

};  // namespace

VideoDecodeStatsDBImpl::VideoDecodeStatsDBImpl() : weak_ptr_factory_(this) {}

VideoDecodeStatsDBImpl::~VideoDecodeStatsDBImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void VideoDecodeStatsDBImpl::Initialize(
    std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db,
    const base::FilePath& dir) {
  DCHECK(db.get());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::move(db);

  // "Simple options" will use the default global cache of 8MB. In the worst
  // case our whole DB will be less than 35K, so we aren't worried about
  // spamming the cache.
  // TODO(chcunningham): Keep an eye on the size as the table evolves.
  db_->Init(kDatabaseClientName, dir, leveldb_proto::CreateSimpleOptions(),
            base::BindOnce(&VideoDecodeStatsDBImpl::OnInit,
                           weak_ptr_factory_.GetWeakPtr()));
}

void VideoDecodeStatsDBImpl::OnInit(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(2) << __func__ << (success ? " succeeded" : " FAILED!");

  db_init_ = true;

  // Can't use DB when initialization fails.
  // TODO(chcunningham): Record UMA.
  if (!success)
    db_.reset();
}

bool VideoDecodeStatsDBImpl::IsDatabaseReady(const std::string& operation) {
  if (!db_init_ || !db_) {
    DVLOG_IF(2, !operation.empty()) << __func__ << " DB is not initialized; "
                                    << "ignoring operation: " << operation;
    return false;
  }

  return true;
}

void VideoDecodeStatsDBImpl::AppendDecodeStats(const VideoDescKey& key,
                                               const DecodeStatsEntry& entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady("AppendDecodeStats")) {
    return;
  }

  DVLOG(3) << __func__ << " Reading key " << KeyToString(key)
           << " from DB with intent to update with " << EntryToString(entry);

  db_->GetEntry(SerializeKey(key),
                base::BindOnce(&VideoDecodeStatsDBImpl::WriteUpdatedEntry,
                               weak_ptr_factory_.GetWeakPtr(), key, entry));
}

void VideoDecodeStatsDBImpl::GetDecodeStats(const VideoDescKey& key,
                                            GetDecodeStatsCB callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsDatabaseReady("GetDecodeStats")) {
    std::move(callback).Run(false, nullptr);
    return;
  }

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

  // TODO(chcunningham): Record UMA.
  if (!read_success) {
    DVLOG(2) << __func__ << " FAILED DB read for " << KeyToString(key)
             << "; ignoring update!";
    return;
  }

  DCHECK(IsDatabaseReady("WriteUpdatedEntry"));

  if (!prev_stats_proto) {
    prev_stats_proto.reset(new DecodeStatsProto());
    prev_stats_proto->set_frames_decoded(0);
    prev_stats_proto->set_frames_dropped(0);
  }

  uint64_t sum_frames_decoded =
      prev_stats_proto->frames_decoded() + entry.frames_decoded;
  uint64_t sum_frames_dropped =
      prev_stats_proto->frames_dropped() + entry.frames_dropped;

  prev_stats_proto->set_frames_decoded(sum_frames_decoded);
  prev_stats_proto->set_frames_dropped(sum_frames_dropped);

  DVLOG(3) << __func__ << " Updating " << KeyToString(key) << " with "
           << EntryToString(entry) << " aggregate:"
           << EntryToString(
                  DecodeStatsEntry(sum_frames_decoded, sum_frames_dropped));

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
    entry = std::make_unique<DecodeStatsEntry>(stats_proto->frames_decoded(),
                                               stats_proto->frames_dropped());
  }

  DVLOG(3) << __func__ << " read " << (success ? "succeeded" : "FAILED!")
           << " entry: " << (entry ? EntryToString(*entry) : "nullptr");

  std::move(callback).Run(success, std::move(entry));
}

}  // namespace media
