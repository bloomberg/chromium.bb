// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/non_blocking_type_commit_contribution.h"

#include <algorithm>

#include "base/values.h"
#include "components/sync/engine/non_blocking_sync_common.h"
#include "components/sync/engine_impl/model_type_worker.h"
#include "components/sync/protocol/proto_value_conversions.h"

namespace syncer {

NonBlockingTypeCommitContribution::NonBlockingTypeCommitContribution(
    const sync_pb::DataTypeContext& context,
    const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
    ModelTypeWorker* worker,
    DataTypeDebugInfoEmitter* debug_info_emitter)
    : worker_(worker),
      context_(context),
      entities_(entities),
      cleaned_up_(false),
      debug_info_emitter_(debug_info_emitter) {}

NonBlockingTypeCommitContribution::~NonBlockingTypeCommitContribution() {
  DCHECK(cleaned_up_);
}

void NonBlockingTypeCommitContribution::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();

  std::copy(entities_.begin(), entities_.end(),
            RepeatedPtrFieldBackInserter(commit_message->mutable_entries()));
  if (!context_.context().empty())
    commit_message->add_client_contexts()->CopyFrom(context_);

  CommitCounters* counters = debug_info_emitter_->GetMutableCommitCounters();
  counters->num_commits_attempted += entities_.size();
}

SyncerError NonBlockingTypeCommitContribution::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    StatusController* status) {
  const sync_pb::CommitResponse& commit_response = response.commit();

  bool unknown_error = false;
  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int successes = 0;

  CommitResponseDataList response_list;

  for (int i = 0; i < entities_.size(); ++i) {
    const sync_pb::CommitResponse_EntryResponse& entry_response =
        commit_response.entryresponse(entries_start_index_ + i);

    switch (entry_response.response_type()) {
      case sync_pb::CommitResponse::INVALID_MESSAGE:
        LOG(ERROR) << "Server reports commit message is invalid.";
        DLOG(ERROR) << "Message was: "
                    << SyncEntityToValue(entities_.Get(i), false).get();
        unknown_error = true;
        break;
      case sync_pb::CommitResponse::CONFLICT:
        DVLOG(1) << "Server reports conflict for commit message.";
        DVLOG(1) << "Message was: "
                 << SyncEntityToValue(entities_.Get(i), false).get();
        ++conflicting_commits;
        break;
      case sync_pb::CommitResponse::SUCCESS: {
        ++successes;
        CommitResponseData response_data;
        response_data.id = entry_response.id_string();
        response_data.client_tag_hash =
            entities_.Get(i).client_defined_unique_tag();
        response_data.response_version = entry_response.version();
        response_list.push_back(response_data);
        break;
      }
      case sync_pb::CommitResponse::OVER_QUOTA:
      case sync_pb::CommitResponse::RETRY:
      case sync_pb::CommitResponse::TRANSIENT_ERROR:
        DLOG(WARNING) << "Entity commit blocked by transient error.";
        ++transient_error_commits;
        break;
      default:
        LOG(ERROR) << "Bad return from ProcessSingleCommitResponse.";
        unknown_error = true;
    }
  }

  CommitCounters* counters = debug_info_emitter_->GetMutableCommitCounters();
  counters->num_commits_success += successes;
  counters->num_commits_conflict += transient_error_commits;
  counters->num_commits_error += transient_error_commits;

  // Send whatever successful responses we did get back to our parent.
  // It's the schedulers job to handle the failures.
  worker_->OnCommitResponse(&response_list);

  // Let the scheduler know about the failures.
  if (unknown_error) {
    return SERVER_RETURN_UNKNOWN_ERROR;
  } else if (transient_error_commits > 0) {
    return SERVER_RETURN_TRANSIENT_ERROR;
  } else if (conflicting_commits > 0) {
    return SERVER_RETURN_CONFLICT;
  } else {
    return SYNCER_OK;
  }
}

void NonBlockingTypeCommitContribution::CleanUp() {
  cleaned_up_ = true;

  debug_info_emitter_->EmitCommitCountersUpdate();
  debug_info_emitter_->EmitStatusCountersUpdate();

  // We could inform our parent NonBlockingCommitContributor that a commit is
  // no longer in progress.  The current implementation doesn't really care
  // either way, so we don't bother sending the signal.
}

size_t NonBlockingTypeCommitContribution::GetNumEntries() const {
  return entities_.size();
}

}  // namespace syncer
