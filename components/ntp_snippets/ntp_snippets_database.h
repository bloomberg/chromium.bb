// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_
#define COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/ntp_snippets/ntp_snippet.h"

namespace base {
class FilePath;
}

namespace ntp_snippets {

class SnippetProto;

class NTPSnippetsDatabase {
 public:
  using SnippetsLoadedCallback = base::Callback<void(NTPSnippet::PtrVector)>;

  NTPSnippetsDatabase(
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner);
  ~NTPSnippetsDatabase();

  // Loads all snippets from storage and passes them to |callback|.
  void Load(const SnippetsLoadedCallback& callback);

  // Adds or updates the given snippet.
  void Save(const NTPSnippet& snippet);
  // Adds or updates all the given snippets.
  void Save(const NTPSnippet::PtrVector& snippets);

  // Deletes the snippet with the given ID.
  void Delete(const std::string& snippet_id);
  // Deletes all the given snippets (identified by their IDs).
  void Delete(const NTPSnippet::PtrVector& snippets);

 private:
  friend class NTPSnippetsDatabaseTest;

  using KeyEntryVector =
      leveldb_proto::ProtoDatabase<SnippetProto>::KeyEntryVector;

  // Callbacks for ProtoDatabase operations.
  void OnDatabaseInited(bool success);
  void OnDatabaseLoaded(const SnippetsLoadedCallback& callback,
                        bool success,
                        std::unique_ptr<std::vector<SnippetProto>> entries);
  void OnDatabaseSaved(bool success);

  void LoadImpl(const SnippetsLoadedCallback& callback);

  void SaveImpl(std::unique_ptr<KeyEntryVector> entries_to_save);

  void DeleteImpl(std::unique_ptr<std::vector<std::string>> keys_to_remove);

  std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetProto>> database_;
  bool database_initialized_;

  std::vector<SnippetsLoadedCallback> pending_load_callbacks_;

  base::WeakPtrFactory<NTPSnippetsDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsDatabase);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_NTP_SNIPPETS_DATABASE_H_
