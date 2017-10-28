// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_IMPL_H_
#define MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_IMPL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"
#include "media/base/video_codecs.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/video_decode_stats_db.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
}  // namespace base

namespace media {

class DecodeStatsProto;

// Factory interface to create a DB instance.
class MEDIA_MOJO_EXPORT VideoDecodeStatsDBImplFactory
    : public VideoDecodeStatsDBFactory {
 public:
  explicit VideoDecodeStatsDBImplFactory(base::FilePath db_dir);
  ~VideoDecodeStatsDBImplFactory() override;
  std::unique_ptr<VideoDecodeStatsDB> CreateDB() override;

 private:
  base::FilePath db_dir_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImplFactory);
};

// LevelDB implementation of VideoDecodeStatsDB. This class is not
// thread safe. All API calls should happen on the same sequence used for
// construction. API callbacks will also occur on this sequence.
class MEDIA_MOJO_EXPORT VideoDecodeStatsDBImpl : public VideoDecodeStatsDB {
 public:
  // Constructs the database. NOTE: must call Initialize() before using.
  // |db| injects the level_db database instance for storing capabilities info.
  // |dir| specifies where to store LevelDB files to disk. LevelDB generates a
  // handful of files, so its recommended to provide a dedicated directory to
  // keep them isolated.
  // |allow_writes| determines whether DB write operations will be allowed. This
  // is a temporary mechanism to disable writes in production until we've added
  // the ability to clear the database via chrome://settings/clearBrowserData.
  VideoDecodeStatsDBImpl(
      std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db,
      const base::FilePath& dir,
      bool allow_writes);
  ~VideoDecodeStatsDBImpl() override;

  // Implement VideoDecodeStatsDB.
  void Initialize(base::OnceCallback<void(bool)> init_cb) override;
  void AppendDecodeStats(const VideoDescKey& key,
                         const DecodeStatsEntry& entry) override;
  void GetDecodeStats(const VideoDescKey& key,
                      GetDecodeStatsCB callback) override;

 private:
  friend class VideoDecodeStatsDBTest;

  // Called when the database has been initialized. Will immediately call
  // |init_cb| to forward |success|.
  void OnInit(base::OnceCallback<void(bool)> init_cb, bool success);

  // Returns true if the DB is successfully initialized.
  bool IsInitialized();

  // Passed as the callback for |OnGotDecodeStats| by |AppendDecodeStats| to
  // update the database once we've read the existing stats entry.
  void WriteUpdatedEntry(const VideoDescKey& key,
                         const DecodeStatsEntry& entry,
                         bool read_success,
                         std::unique_ptr<DecodeStatsProto> prev_stats_proto);

  // Called when the database has been modified after a call to
  // |WriteUpdatedEntry|.
  void OnEntryUpdated(bool success);

  // Called when GetDecodeStats() operation was performed. |callback|
  // will be run with |success| and a |DecodeStatsEntry| created from
  // |stats_proto| or nullptr if no entry was found for the requested key.
  void OnGotDecodeStats(
      GetDecodeStatsCB callback,
      bool success,
      std::unique_ptr<DecodeStatsProto> capabilities_info_proto);

  // Indicates whether initialization is completed. Does not indicate whether it
  // was successful. Failed initialization is signaled by setting |db_| to null.
  bool db_init_ = false;

  // ProtoDatabase instance. Set to nullptr if fatal database error is
  // encountered.
  std::unique_ptr<leveldb_proto::ProtoDatabase<DecodeStatsProto>> db_;

  // Directory where levelDB should store database files.
  base::FilePath db_dir_;

  // Determines whether DB write operations will be allowed. This is a temporary
  // mechanism to disable writes in production until we've added the ability to
  // clear the database via chrome://settings/clearBrowserData.
  bool allow_writes_;

  // Ensures all access to class members come on the same sequence. API calls
  // and callbacks should occur on the same sequence used during construction.
  // LevelDB operations happen on a separate task runner, but all LevelDB
  // callbacks to this happen on the checked sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<VideoDecodeStatsDBImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeStatsDBImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_VIDEO_DECODE_STATS_DB_IMPL_H_
