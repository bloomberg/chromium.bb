// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/ntp_snippets_database.h"

#include <utility>

#include "base/files/file_path.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "components/ntp_snippets/proto/ntp_snippets.pb.h"

using leveldb_proto::ProtoDatabaseImpl;

namespace {
// Statistics are logged to UMA with this string as part of histogram name. They
// can all be found under LevelDB.*.NTPSnippets. Changing this needs to
// synchronize with histograms.xml, AND will also become incompatible with older
// browsers still reporting the previous values.
const char kDatabaseUMAClientName[] = "NTPSnippets";
}

namespace ntp_snippets {

NTPSnippetsDatabase::NTPSnippetsDatabase(
    const base::FilePath& database_dir,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : database_(
          new ProtoDatabaseImpl<SnippetProto>(std::move(file_task_runner))),
      database_initialized_(false),
      weak_ptr_factory_(this) {
  database_->Init(kDatabaseUMAClientName, database_dir,
                  base::Bind(&NTPSnippetsDatabase::OnDatabaseInited,
                             weak_ptr_factory_.GetWeakPtr()));
}

NTPSnippetsDatabase::~NTPSnippetsDatabase() {}

void NTPSnippetsDatabase::Load(const SnippetsLoadedCallback& callback) {
  if (database_ && database_initialized_)
    LoadImpl(callback);
  else
    pending_load_callbacks_.emplace_back(callback);
}

void NTPSnippetsDatabase::Save(const NTPSnippet& snippet) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  entries_to_save->emplace_back(snippet.id(), snippet.ToProto());
  SaveImpl(std::move(entries_to_save));
}

void NTPSnippetsDatabase::Save(const NTPSnippet::PtrVector& snippets) {
  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets)
    entries_to_save->emplace_back(snippet->id(), snippet->ToProto());
  SaveImpl(std::move(entries_to_save));
}

void NTPSnippetsDatabase::Delete(const std::string& snippet_id) {
  DeleteImpl(base::WrapUnique(new std::vector<std::string>(1, snippet_id)));
}

void NTPSnippetsDatabase::Delete(const NTPSnippet::PtrVector& snippets) {
  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  for (const std::unique_ptr<NTPSnippet>& snippet : snippets)
    keys_to_remove->emplace_back(snippet->id());
  DeleteImpl(std::move(keys_to_remove));
}

void NTPSnippetsDatabase::OnDatabaseInited(bool success) {
  DCHECK(!database_initialized_);
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase init failed.";
    database_.reset();
    return;
  }
  database_initialized_ = true;
  for (const SnippetsLoadedCallback& callback : pending_load_callbacks_)
    LoadImpl(callback);
}

void NTPSnippetsDatabase::OnDatabaseLoaded(
    const SnippetsLoadedCallback& callback,
    bool success,
    std::unique_ptr<std::vector<SnippetProto>> entries) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase load failed.";
    database_.reset();
    return;
  }

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());

  NTPSnippet::PtrVector snippets;
  for (const SnippetProto& proto : *entries) {
    std::unique_ptr<NTPSnippet> snippet = NTPSnippet::CreateFromProto(proto);
    if (snippet) {
      snippets.emplace_back(std::move(snippet));
    } else {
      LOG(WARNING) << "Invalid proto from DB " << proto.id();
      keys_to_remove->emplace_back(proto.id());
    }
  }

  callback.Run(std::move(snippets));

  // If any of the snippet protos couldn't be converted to actual snippets,
  // clean them up now.
  if (!keys_to_remove->empty())
    DeleteImpl(std::move(keys_to_remove));
}

void NTPSnippetsDatabase::OnDatabaseSaved(bool success) {
  if (!success) {
    DVLOG(1) << "NTPSnippetsDatabase save failed.";
    database_.reset();
  }
}

void NTPSnippetsDatabase::LoadImpl(const SnippetsLoadedCallback& callback) {
  DCHECK(database_);
  DCHECK(database_initialized_);
  database_->LoadEntries(base::Bind(&NTPSnippetsDatabase::OnDatabaseLoaded,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    callback));
}

void NTPSnippetsDatabase::SaveImpl(
    std::unique_ptr<KeyEntryVector> entries_to_save) {
  if (!database_ || !database_initialized_)
    return;

  std::unique_ptr<std::vector<std::string>> keys_to_remove(
      new std::vector<std::string>());
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&NTPSnippetsDatabase::OnDatabaseSaved,
                                      weak_ptr_factory_.GetWeakPtr()));
}

void NTPSnippetsDatabase::DeleteImpl(
    std::unique_ptr<std::vector<std::string>> keys_to_remove) {
  if (!database_ || !database_initialized_)
    return;

  std::unique_ptr<KeyEntryVector> entries_to_save(new KeyEntryVector());
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_remove),
                           base::Bind(&NTPSnippetsDatabase::OnDatabaseSaved,
                                      weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ntp_snippets
