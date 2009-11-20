// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock ServerConnectionManager class for use in client regression tests.

#include "chrome/test/sync/engine/mock_server_connection.h"

#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/test/sync/engine/test_id_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::HttpResponse;
using browser_sync::ServerConnectionManager;
using browser_sync::SyncerProtoUtil;
using browser_sync::TestIdFactory;
using std::map;
using std::string;
using sync_pb::ClientToServerMessage;
using sync_pb::ClientToServerResponse;
using sync_pb::CommitMessage;
using sync_pb::CommitResponse;
using sync_pb::CommitResponse_EntryResponse;
using sync_pb::GetUpdatesMessage;
using sync_pb::SyncEntity;
using syncable::DirectoryManager;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

MockConnectionManager::MockConnectionManager(DirectoryManager* dirmgr,
                                             const string& name)
    : ServerConnectionManager("unused", 0, false, "version", "id"),
      conflict_all_commits_(false),
      conflict_n_commits_(0),
      next_new_id_(10000),
      store_birthday_("Store BDay!"),
      store_birthday_sent_(false),
      client_stuck_(false),
      commit_time_rename_prepended_string_(""),
      fail_next_postbuffer_(false),
      directory_manager_(dirmgr),
      directory_name_(name),
      mid_commit_callback_(NULL),
      mid_commit_observer_(NULL),
      throttling_(false),
      fail_non_periodic_get_updates_(false),
      client_command_(NULL),
      next_position_in_parent_(2) {
    server_reachable_ = true;
};

MockConnectionManager::~MockConnectionManager() {
  for (size_t i = 0; i < commit_messages_.size(); i++)
    delete commit_messages_[i];
}

void MockConnectionManager::SetCommitTimeRename(string prepend) {
  commit_time_rename_prepended_string_ = prepend;
}

void MockConnectionManager::SetMidCommitCallback(Closure* callback) {
  mid_commit_callback_ = callback;
}

void MockConnectionManager::SetMidCommitObserver(
    MockConnectionManager::MidCommitObserver* observer) {
    mid_commit_observer_ = observer;
}

bool MockConnectionManager::PostBufferToPath(const PostBufferParams* params,
                                             const string& path,
                                             const string& auth_token) {
  ClientToServerMessage post;
  CHECK(post.ParseFromString(params->buffer_in));
  client_stuck_ = post.sync_problem_detected();
  ClientToServerResponse response;
  response.Clear();

  ScopedDirLookup directory(directory_manager_, directory_name_);
  // For any non-AUTHENTICATE requests, a valid directory should be set up.
  // If the Directory's locked when we do this, it's a problem as in normal
  // use this function could take a while to return because it accesses the
  // network. As we can't test this we do the next best thing and hang here
  // when there's an issue.
  if (post.message_contents() != ClientToServerMessage::AUTHENTICATE) {
    CHECK(directory.good());
    WriteTransaction wt(directory, syncable::UNITTEST, __FILE__, __LINE__);
  }

  if (fail_next_postbuffer_) {
    fail_next_postbuffer_ = false;
    return false;
  }
  // Default to an ok connection.
  params->response->server_status = HttpResponse::SERVER_CONNECTION_OK;
  response.set_store_birthday(store_birthday_);
  if (post.has_store_birthday() && post.store_birthday() != store_birthday_) {
    response.set_error_code(ClientToServerResponse::NOT_MY_BIRTHDAY);
    response.set_error_message("Merry Unbirthday!");
    response.SerializeToString(params->buffer_out);
    store_birthday_sent_ = true;
    return true;
  }
  bool result = true;
  EXPECT_TRUE(!store_birthday_sent_ || post.has_store_birthday() ||
              post.message_contents() == ClientToServerMessage::AUTHENTICATE);
  store_birthday_sent_ = true;

  if (post.message_contents() == ClientToServerMessage::COMMIT) {
    ProcessCommit(&post, &response);
  } else if (post.message_contents() == ClientToServerMessage::GET_UPDATES) {
    ProcessGetUpdates(&post, &response);
  } else if (post.message_contents() == ClientToServerMessage::AUTHENTICATE) {
    ProcessAuthenticate(&post, &response, auth_token);
  } else {
    EXPECT_TRUE(false) << "Unknown/unsupported ClientToServerMessage";
    return false;
  }
  if (client_command_.get()) {
    response.mutable_client_command()->CopyFrom(*client_command_.get());
  }

  {
    AutoLock throttle_lock(throttle_lock_);
    if (throttling_) {
        response.set_error_code(ClientToServerResponse::THROTTLED);
        throttling_ = false;
    }
  }

  response.SerializeToString(params->buffer_out);
  if (mid_commit_callback_) {
    mid_commit_callback_->Run();
  }
  if (mid_commit_observer_) {
    mid_commit_observer_->Observe();
  }

  return result;
}

bool MockConnectionManager::IsServerReachable() {
  return true;
}

bool MockConnectionManager::IsUserAuthenticated() {
  return true;
}

void MockConnectionManager::ResetUpdates() {
  updates_.Clear();
}

namespace {

void AddDefaultBookmarkData(SyncEntity* entity, bool is_folder) {
  sync_pb::SyncEntity_BookmarkData* data = entity->mutable_bookmarkdata();
  data->set_bookmark_folder(is_folder);

  if (!is_folder) {
    data->set_bookmark_url("http://google.com");
  }
}

}  // namespace

SyncEntity* MockConnectionManager::AddUpdateDirectory(int id,
                                                      int parent_id,
                                                      string name,
                                                      int64 version,
                                                      int64 sync_ts) {
  return AddUpdateDirectory(TestIdFactory::FromNumber(id),
                            TestIdFactory::FromNumber(parent_id),
                            name,
                            version,
                            sync_ts);
}

sync_pb::ClientCommand* MockConnectionManager::GetNextClientCommand() {
  if (!client_command_.get())
    client_command_.reset(new sync_pb::ClientCommand());
  return client_command_.get();
}

SyncEntity* MockConnectionManager::AddUpdateBookmark(int id, int parent_id,
                                                     string name, int64 version,
                                                     int64 sync_ts) {
  return AddUpdateBookmark(TestIdFactory::FromNumber(id),
                           TestIdFactory::FromNumber(parent_id),
                           name,
                           version,
                           sync_ts);
}

SyncEntity* MockConnectionManager::AddUpdateFull(string id, string parent_id,
                                                 string name, int64 version,
                                                 int64 sync_ts, bool is_dir) {
  SyncEntity* ent = updates_.add_entries();
  ent->set_id_string(id);
  ent->set_parent_id_string(parent_id);
  ent->set_name(name);
  ent->set_version(version);
  ent->set_sync_timestamp(sync_ts);
  ent->set_mtime(sync_ts);
  ent->set_ctime(1);
  ent->set_position_in_parent(GeneratePositionInParent());
  AddDefaultBookmarkData(ent, is_dir);

  return ent;
}

SyncEntity* MockConnectionManager::AddUpdateDirectory(string id,
                                                      string parent_id,
                                                      string name,
                                                      int64 version,
                                                      int64 sync_ts) {
  return AddUpdateFull(id, parent_id, name, version, sync_ts, true);
}

SyncEntity* MockConnectionManager::AddUpdateBookmark(string id,
                                                     string parent_id,
                                                     string name, int64 version,
                                                     int64 sync_ts) {
  return AddUpdateFull(id, parent_id, name, version, sync_ts, false);
}

void MockConnectionManager::SetLastUpdateDeleted() {
  GetMutableLastUpdate()->set_deleted(true);
}

void MockConnectionManager::SetLastUpdateOriginatorFields(
    const string& client_id,
    const string& entry_id) {
  GetMutableLastUpdate()->set_originator_cache_guid(client_id);
  GetMutableLastUpdate()->set_originator_client_item_id(entry_id);
}

void MockConnectionManager::SetLastUpdateSingletonTag(const string& tag) {
  GetMutableLastUpdate()->set_singleton_tag(tag);
}

void MockConnectionManager::SetLastUpdatePosition(int64 server_position) {
  GetMutableLastUpdate()->set_position_in_parent(server_position);
}

void MockConnectionManager::SetNewTimestamp(int64 ts) {
  updates_.set_new_timestamp(ts);
}

void MockConnectionManager::SetChangesRemaining(int64 timestamp) {
  updates_.set_changes_remaining(timestamp);
}

void MockConnectionManager::ProcessGetUpdates(ClientToServerMessage* csm,
    ClientToServerResponse* response) {
  CHECK(csm->has_get_updates());
  ASSERT_EQ(csm->message_contents(), ClientToServerMessage::GET_UPDATES);
  const GetUpdatesMessage& gu = csm->get_updates();
  EXPECT_TRUE(gu.has_from_timestamp());
  if (fail_non_periodic_get_updates_) {
    EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::PERIODIC,
              gu.caller_info().source());
  }

  // TODO(sync): filter results dependant on timestamp? or check limits?
  response->mutable_get_updates()->CopyFrom(updates_);
  ResetUpdates();
}

void MockConnectionManager::ProcessAuthenticate(
    ClientToServerMessage* csm,
    ClientToServerResponse* response,
    const std::string& auth_token) {
  ASSERT_EQ(csm->message_contents(), ClientToServerMessage::AUTHENTICATE);
  EXPECT_FALSE(auth_token.empty());

  if (auth_token != valid_auth_token_) {
    response->set_error_code(ClientToServerResponse::AUTH_INVALID);
    return;
  }

  response->set_error_code(ClientToServerResponse::SUCCESS);
  response->mutable_authenticate()->CopyFrom(auth_response_);
  auth_response_.Clear();
}

void MockConnectionManager::SetAuthenticationResponseInfo(
    const std::string& valid_auth_token,
    const std::string& user_display_name,
    const std::string& user_display_email,
    const std::string& user_obfuscated_id) {
  valid_auth_token_ = valid_auth_token;
  sync_pb::UserIdentification* user = auth_response_.mutable_user();
  user->set_display_name(user_display_name);
  user->set_email(user_display_email);
  user->set_obfuscated_id(user_obfuscated_id);
}

bool MockConnectionManager::ShouldConflictThisCommit() {
  bool conflict = false;
  if (conflict_all_commits_) {
    conflict = true;
  } else if (conflict_n_commits_ > 0) {
    conflict = true;
    --conflict_n_commits_;
  }
  return conflict;
}

void MockConnectionManager::ProcessCommit(ClientToServerMessage* csm,
    ClientToServerResponse* response_buffer) {
  CHECK(csm->has_commit());
  ASSERT_EQ(csm->message_contents(), ClientToServerMessage::COMMIT);
  map <string, string> changed_ids;
  const CommitMessage& commit_message = csm->commit();
  CommitResponse* commit_response = response_buffer->mutable_commit();
  commit_messages_.push_back(new CommitMessage);
  commit_messages_.back()->CopyFrom(commit_message);
  map<string, CommitResponse_EntryResponse*> response_map;
  for (int i = 0; i < commit_message.entries_size() ; i++) {
    const sync_pb::SyncEntity& entry = commit_message.entries(i);
    CHECK(entry.has_id_string());
    string id = entry.id_string();
    ASSERT_LT(entry.name().length(), 256ul) << " name probably too long. True "
        "server name checking not implemented";
    if (entry.version() == 0) {
      // Relies on our new item string id format. (string representation of a
      // negative number).
      committed_ids_.push_back(syncable::Id::CreateFromClientString(id));
    } else {
      committed_ids_.push_back(syncable::Id::CreateFromServerId(id));
    }
    if (response_map.end() == response_map.find(id))
      response_map[id] = commit_response->add_entryresponse();
    CommitResponse_EntryResponse* er = response_map[id];
    if (ShouldConflictThisCommit()) {
      er->set_response_type(CommitResponse::CONFLICT);
      continue;
    }
    er->set_response_type(CommitResponse::SUCCESS);
    er->set_version(entry.version() + 1);
    if (!commit_time_rename_prepended_string_.empty()) {
      // Commit time rename sent down from the server.
      er->set_name(commit_time_rename_prepended_string_ + entry.name());
    }
    string parent_id = entry.parent_id_string();
    // Remap id's we've already assigned.
    if (changed_ids.end() != changed_ids.find(parent_id)) {
      parent_id = changed_ids[parent_id];
      er->set_parent_id_string(parent_id);
    }
    if (entry.has_version() && 0 != entry.version()) {
      er->set_id_string(id);  // Allows verification.
    } else {
      string new_id = StringPrintf("mock_server:%d", next_new_id_++);
      changed_ids[id] = new_id;
      er->set_id_string(new_id);
    }
  }
}

SyncEntity* MockConnectionManager::AddUpdateDirectory(
    syncable::Id id, syncable::Id parent_id, string name, int64 version,
    int64 sync_ts) {
  return AddUpdateDirectory(id.GetServerId(), parent_id.GetServerId(),
                            name, version, sync_ts);
}

SyncEntity* MockConnectionManager::AddUpdateBookmark(
    syncable::Id id, syncable::Id parent_id, string name, int64 version,
    int64 sync_ts) {
  return AddUpdateBookmark(id.GetServerId(), parent_id.GetServerId(),
                           name, version, sync_ts);
}

void MockConnectionManager::AddUpdateExtendedAttributes(SyncEntity* ent,
    string* xattr_key, syncable::Blob* xattr_value, int xattr_count) {
  sync_pb::ExtendedAttributes* mutable_extended_attributes =
      ent->mutable_extended_attributes();
  for (int i = 0; i < xattr_count; i++) {
    sync_pb::ExtendedAttributes_ExtendedAttribute* extended_attribute =
        mutable_extended_attributes->add_extendedattribute();
    extended_attribute->set_key(xattr_key[i]);
    SyncerProtoUtil::CopyBlobIntoProtoBytes(xattr_value[i],
        extended_attribute->mutable_value());
  }
}

SyncEntity* MockConnectionManager::GetMutableLastUpdate() {
  DCHECK(updates_.entries_size() > 0);
  return updates_.mutable_entries()->Mutable(updates_.entries_size() - 1);
}

const CommitMessage& MockConnectionManager::last_sent_commit() const {
  DCHECK(!commit_messages_.empty());
  return *commit_messages_.back();
}

void MockConnectionManager::ThrottleNextRequest(
    ThrottleRequestVisitor* visitor) {
  AutoLock lock(throttle_lock_);
  throttling_ = true;
  if (visitor)
    visitor->VisitAtomically();
}

