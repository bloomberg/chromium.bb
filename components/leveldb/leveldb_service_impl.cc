// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb/leveldb_service_impl.h"

#include "components/leveldb/env_mojo.h"
#include "components/leveldb/leveldb_database_impl.h"
#include "components/leveldb/util.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/helpers/memenv/memenv.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace leveldb {

LevelDBServiceImpl::LevelDBServiceImpl() : thread_(new LevelDBFileThread) {}

LevelDBServiceImpl::~LevelDBServiceImpl() {}

void LevelDBServiceImpl::Open(filesystem::DirectoryPtr directory,
                              const mojo::String& dbname,
                              leveldb::LevelDBDatabaseRequest database,
                              const OpenCallback& callback) {
  // This is the place where we open a database.
  leveldb::Options options;
  options.create_if_missing = true;
  options.paranoid_checks = true;
  // TODO(erg): Do we need a filter policy?
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;
  options.compression = leveldb::kSnappyCompression;

  // For info about the troubles we've run into with this parameter, see:
  // https://code.google.com/p/chromium/issues/detail?id=227313#c11
  options.max_open_files = 80;

  // Register our directory with the file thread.
  LevelDBFileThread::OpaqueDir* dir =
      thread_->RegisterDirectory(std::move(directory));

  scoped_ptr<MojoEnv> env_mojo(new MojoEnv(thread_, dir));
  options.env = env_mojo.get();

  leveldb::DB* db = nullptr;
  leveldb::Status s = leveldb::DB::Open(options, dbname.To<std::string>(), &db);

  if (s.ok()) {
    new LevelDBDatabaseImpl(std::move(database), std::move(env_mojo),
                            scoped_ptr<leveldb::DB>(db));
  }

  callback.Run(LeveldbStatusToError(s));
}

void LevelDBServiceImpl::OpenInMemory(leveldb::LevelDBDatabaseRequest database,
                                      const OpenCallback& callback) {
  leveldb::Options options;
  options.create_if_missing = true;
  options.max_open_files = 0;  // Use minimum.
  options.reuse_logs = leveldb_env::kDefaultLogReuseOptionValue;

  scoped_ptr<leveldb::Env> env(leveldb::NewMemEnv(leveldb::Env::Default()));
  options.env = env.get();

  leveldb::DB* db = nullptr;
  leveldb::Status s = leveldb::DB::Open(options, "", &db);

  if (s.ok()) {
    new LevelDBDatabaseImpl(std::move(database), std::move(env),
                            scoped_ptr<leveldb::DB>(db));
  }

  callback.Run(LeveldbStatusToError(s));
}

}  // namespace leveldb
