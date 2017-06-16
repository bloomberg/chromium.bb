// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/engine/mock_model_type_worker.h"

#include "base/logging.h"
#include "components/sync/base/model_type.h"
#include "components/sync/syncable/syncable_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

}  // namespace

MockModelTypeWorker::MockModelTypeWorker(
    const sync_pb::ModelTypeState& model_type_state,
    ModelTypeProcessor* processor)
    : model_type_state_(model_type_state), processor_(processor) {
  model_type_state_.set_initial_sync_done(true);
}

MockModelTypeWorker::~MockModelTypeWorker() {}

void MockModelTypeWorker::EnqueueForCommit(const CommitRequestDataList& list) {
  // Verify that all request entities have valid id, version combinations.
  for (const CommitRequestData& commit_request_data : list) {
    EXPECT_TRUE(commit_request_data.base_version == -1 ||
                !commit_request_data.entity->id.empty());
  }
  pending_commits_.push_back(list);
}

size_t MockModelTypeWorker::GetNumPendingCommits() const {
  return pending_commits_.size();
}

CommitRequestDataList MockModelTypeWorker::GetNthPendingCommit(size_t n) const {
  DCHECK_LT(n, GetNumPendingCommits());
  return pending_commits_[n];
}

bool MockModelTypeWorker::HasPendingCommitForHash(
    const std::string& tag_hash) const {
  for (const CommitRequestDataList& commit : pending_commits_) {
    for (const CommitRequestData& data : commit) {
      if (data.entity->client_tag_hash == tag_hash) {
        return true;
      }
    }
  }
  return false;
}

CommitRequestData MockModelTypeWorker::GetLatestPendingCommitForHash(
    const std::string& tag_hash) const {
  // Iterate backward through the sets of commit requests to find the most
  // recent one that applies to the specified tag_hash.
  for (auto rev_it = pending_commits_.rbegin();
       rev_it != pending_commits_.rend(); ++rev_it) {
    for (const CommitRequestData& data : *rev_it) {
      if (data.entity->client_tag_hash == tag_hash) {
        return data;
      }
    }
  }
  NOTREACHED() << "Could not find commit for tag hash " << tag_hash << ".";
  return CommitRequestData();
}

void MockModelTypeWorker::VerifyNthPendingCommit(
    size_t n,
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  const CommitRequestDataList& list = GetNthPendingCommit(n);
  ASSERT_EQ(1U, list.size());
  const EntityData& data = list[0].entity.value();
  EXPECT_EQ(tag_hash, data.client_tag_hash);
  EXPECT_EQ(specifics.SerializeAsString(), data.specifics.SerializeAsString());
}

void MockModelTypeWorker::VerifyPendingCommits(
    const std::vector<std::string>& tag_hashes) {
  EXPECT_EQ(tag_hashes.size(), GetNumPendingCommits());
  for (size_t i = 0; i < tag_hashes.size(); i++) {
    const CommitRequestDataList& commits = GetNthPendingCommit(i);
    EXPECT_EQ(1U, commits.size());
    EXPECT_EQ(tag_hashes[i], commits[0].entity->client_tag_hash)
        << "Hash for tag " << tag_hashes[i] << " doesn't match.";
  }
}

void MockModelTypeWorker::UpdateFromServer() {
  processor_->OnUpdateReceived(model_type_state_, UpdateResponseDataList());
}

void MockModelTypeWorker::UpdateFromServer(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  UpdateFromServer(tag_hash, specifics, 1);
}

void MockModelTypeWorker::UpdateFromServer(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset) {
  UpdateFromServer(tag_hash, specifics, version_offset,
                   model_type_state_.encryption_key_name());
}

void MockModelTypeWorker::UpdateFromServer(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset,
    const std::string& ekn) {
  UpdateResponseDataList updates;
  updates.push_back(
      GenerateUpdateData(tag_hash, specifics, version_offset, ekn));
  UpdateFromServer(updates);
}

void MockModelTypeWorker::UpdateFromServer(
    const UpdateResponseDataList& updates) {
  processor_->OnUpdateReceived(model_type_state_, updates);
}

UpdateResponseData MockModelTypeWorker::GenerateUpdateData(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics,
    int64_t version_offset,
    const std::string& ekn) {
  // Overwrite the existing server version if this is the new highest version.
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + version_offset;
  if (version > old_version) {
    SetServerVersion(tag_hash, version);
  }

  EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  data.specifics = specifics;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.modification_time =
      data.creation_time + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = data.specifics.preference().name();

  UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = version;
  response_data.encryption_key_name = ekn;

  return response_data;
}

UpdateResponseData MockModelTypeWorker::GenerateUpdateData(
    const std::string& tag_hash,
    const sync_pb::EntitySpecifics& specifics) {
  return GenerateUpdateData(tag_hash, specifics, 1,
                            model_type_state_.encryption_key_name());
}

void MockModelTypeWorker::TombstoneFromServer(const std::string& tag_hash) {
  int64_t old_version = GetServerVersion(tag_hash);
  int64_t version = old_version + 1;
  SetServerVersion(tag_hash, version);

  EntityData data;
  data.id = GenerateId(tag_hash);
  data.client_tag_hash = tag_hash;
  // These elements should have no effect on behavior, but we set them anyway
  // so we can test they are properly copied around the system if we want to.
  data.creation_time = base::Time::UnixEpoch() + base::TimeDelta::FromDays(1);
  data.modification_time =
      data.creation_time + base::TimeDelta::FromSeconds(version);
  data.non_unique_name = "Name Non Unique";

  UpdateResponseData response_data;
  response_data.entity = data.PassToPtr();
  response_data.response_version = version;
  response_data.encryption_key_name = model_type_state_.encryption_key_name();

  UpdateResponseDataList list;
  list.push_back(response_data);
  processor_->OnUpdateReceived(model_type_state_, list);
}

void MockModelTypeWorker::AckOnePendingCommit() {
  CommitResponseDataList list;
  for (const CommitRequestData& data : pending_commits_.front()) {
    list.push_back(SuccessfulCommitResponse(data));
  }
  pending_commits_.pop_front();
  processor_->OnCommitCompleted(model_type_state_, list);
}

CommitResponseData MockModelTypeWorker::SuccessfulCommitResponse(
    const CommitRequestData& request_data) {
  const EntityData& entity = request_data.entity.value();
  const std::string& client_tag_hash = entity.client_tag_hash;

  CommitResponseData response_data;

  if (request_data.base_version == kUncommittedVersion) {
    // Server assigns new ID to newly committed items.
    DCHECK(entity.id.empty());
    response_data.id = GenerateId(client_tag_hash);
  } else {
    // Otherwise we reuse the ID from the request.
    response_data.id = entity.id;
  }

  response_data.client_tag_hash = client_tag_hash;
  response_data.sequence_number = request_data.sequence_number;
  response_data.specifics_hash = request_data.specifics_hash;

  // Increment the server version on successful commit.
  int64_t version = GetServerVersion(client_tag_hash);
  version++;
  SetServerVersion(client_tag_hash, version);

  response_data.response_version = version;

  return response_data;
}

void MockModelTypeWorker::UpdateWithEncryptionKey(const std::string& ekn) {
  UpdateWithEncryptionKey(ekn, UpdateResponseDataList());
}

void MockModelTypeWorker::UpdateWithEncryptionKey(
    const std::string& ekn,
    const UpdateResponseDataList& update) {
  model_type_state_.set_encryption_key_name(ekn);
  processor_->OnUpdateReceived(model_type_state_, update);
}

std::string MockModelTypeWorker::GenerateId(const std::string& tag_hash) {
  return "FakeId:" + tag_hash;
}

int64_t MockModelTypeWorker::GetServerVersion(const std::string& tag_hash) {
  std::map<const std::string, int64_t>::const_iterator it;
  it = server_versions_.find(tag_hash);
  if (it == server_versions_.end()) {
    return 0;
  } else {
    return it->second;
  }
}

void MockModelTypeWorker::SetServerVersion(const std::string& tag_hash,
                                           int64_t version) {
  server_versions_[tag_hash] = version;
}

}  // namespace syncer
