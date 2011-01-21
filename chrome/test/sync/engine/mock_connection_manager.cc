// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Mock ServerConnectionManager class for use in client regression tests.

#include "chrome/test/sync/engine/mock_connection_manager.h"

#include "chrome/browser/sync/engine/syncer_proto_util.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
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
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;
using syncable::ModelType;
using syncable::ScopedDirLookup;
using syncable::WriteTransaction;

MockConnectionManager::MockConnectionManager(DirectoryManager* dirmgr,
                                             const string& name)
    : ServerConnectionManager("unused", 0, false, "version"),
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
      mid_commit_observer_(NULL),
      throttling_(false),
      fail_with_auth_invalid_(false),
      fail_non_periodic_get_updates_(false),
      client_command_(NULL),
      next_position_in_parent_(2),
      use_legacy_bookmarks_protocol_(false),
      num_get_updates_requests_(0) {
  server_reachable_ = true;
  SetNewTimestamp(0);
};

MockConnectionManager::~MockConnectionManager() {
  EXPECT_TRUE(update_queue_.empty()) << "Unfetched updates.";
}

void MockConnectionManager::SetCommitTimeRename(string prepend) {
  commit_time_rename_prepended_string_ = prepend;
}

void MockConnectionManager::SetMidCommitCallback(Callback0::Type* callback) {
  mid_commit_callback_.reset(callback);
}

void MockConnectionManager::SetMidCommitObserver(
    MockConnectionManager::MidCommitObserver* observer) {
    mid_commit_observer_ = observer;
}

bool MockConnectionManager::PostBufferToPath(const PostBufferParams* params,
    const string& path,
    const string& auth_token,
    browser_sync::ScopedServerStatusWatcher* watcher) {
  ClientToServerMessage post;
  CHECK(post.ParseFromString(params->buffer_in));
  last_request_.CopyFrom(post);
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

  if (!server_reachable_) {
    params->response->server_status = HttpResponse::CONNECTION_UNAVAILABLE;
    return false;
  }

  // Default to an ok connection.
  params->response->server_status = HttpResponse::SERVER_CONNECTION_OK;
  response.set_error_code(ClientToServerResponse::SUCCESS);
  const string current_store_birthday = store_birthday();
  response.set_store_birthday(current_store_birthday);
  if (post.has_store_birthday() && post.store_birthday() !=
      current_store_birthday) {
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
  } else if (post.message_contents() == ClientToServerMessage::CLEAR_DATA) {
    ProcessClearData(&post, &response);
  } else {
    EXPECT_TRUE(false) << "Unknown/unsupported ClientToServerMessage";
    return false;
  }
  if (client_command_.get()) {
    response.mutable_client_command()->CopyFrom(*client_command_.get());
  }

  {
    base::AutoLock lock(response_code_override_lock_);
    if (throttling_) {
      response.set_error_code(ClientToServerResponse::THROTTLED);
      throttling_ = false;
    }

    if (fail_with_auth_invalid_)
      response.set_error_code(ClientToServerResponse::AUTH_INVALID);
  }

  response.SerializeToString(params->buffer_out);
  if (post.message_contents() == ClientToServerMessage::COMMIT &&
      mid_commit_callback_.get()) {
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

sync_pb::GetUpdatesResponse* MockConnectionManager::GetUpdateResponse() {
  if (update_queue_.empty()) {
    NextUpdateBatch();
  }
  return &update_queue_.back();
}

void MockConnectionManager::AddDefaultBookmarkData(sync_pb::SyncEntity* entity,
                                                   bool is_folder) {
  if (use_legacy_bookmarks_protocol_) {
    sync_pb::SyncEntity_BookmarkData* data = entity->mutable_bookmarkdata();
    data->set_bookmark_folder(is_folder);

    if (!is_folder) {
      data->set_bookmark_url("http://google.com");
    }
  } else {
    entity->set_folder(is_folder);
    entity->mutable_specifics()->MutableExtension(sync_pb::bookmark);
    if (!is_folder) {
      entity->mutable_specifics()->MutableExtension(sync_pb::bookmark)->
          set_url("http://google.com");
    }
  }
}

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
  SyncEntity* ent = GetUpdateResponse()->add_entries();
  ent->set_id_string(id);
  ent->set_parent_id_string(parent_id);
  ent->set_non_unique_name(name);
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

SyncEntity* MockConnectionManager::AddUpdateFromLastCommit() {
  EXPECT_EQ(1, last_sent_commit().entries_size());
  EXPECT_EQ(1, last_commit_response().entryresponse_size());
  EXPECT_EQ(CommitResponse::SUCCESS,
      last_commit_response().entryresponse(0).response_type());

  if (last_sent_commit().entries(0).deleted()) {
    AddUpdateTombstone(syncable::Id::CreateFromServerId(
        last_sent_commit().entries(0).id_string()));
  } else {
    SyncEntity* ent = GetUpdateResponse()->add_entries();
    ent->CopyFrom(last_sent_commit().entries(0));
    ent->clear_insert_after_item_id();
    ent->clear_old_parent_id();
    ent->set_position_in_parent(
        last_commit_response().entryresponse(0).position_in_parent());
    ent->set_version(
        last_commit_response().entryresponse(0).version());
    ent->set_id_string(
        last_commit_response().entryresponse(0).id_string());
    // Tests don't currently care about the following:
    // originator_cache_guid, originator_client_item_id, parent_id_string,
    // name, non_unique_name.
  }
  return GetMutableLastUpdate();
}

void MockConnectionManager::AddUpdateTombstone(const syncable::Id& id) {
  // Tombstones have only the ID set and dummy values for the required fields.
  SyncEntity* ent = GetUpdateResponse()->add_entries();
  ent->set_id_string(id.GetServerId());
  ent->set_version(0);
  ent->set_name("");
  ent->set_deleted(true);
}

void MockConnectionManager::SetLastUpdateDeleted() {
  // Tombstones have only the ID set.  Wipe anything else.
  string id_string = GetMutableLastUpdate()->id_string();
  GetUpdateResponse()->mutable_entries()->RemoveLast();
  AddUpdateTombstone(syncable::Id::CreateFromServerId(id_string));
}

void MockConnectionManager::SetLastUpdateOriginatorFields(
    const string& client_id,
    const string& entry_id) {
  GetMutableLastUpdate()->set_originator_cache_guid(client_id);
  GetMutableLastUpdate()->set_originator_client_item_id(entry_id);
}

void MockConnectionManager::SetLastUpdateServerTag(const string& tag) {
  GetMutableLastUpdate()->set_server_defined_unique_tag(tag);
}

void MockConnectionManager::SetLastUpdateClientTag(const string& tag) {
  GetMutableLastUpdate()->set_client_defined_unique_tag(tag);
}

void MockConnectionManager::SetLastUpdatePosition(int64 server_position) {
  GetMutableLastUpdate()->set_position_in_parent(server_position);
}

void MockConnectionManager::SetNewTimestamp(int ts) {
  next_token_ = base::StringPrintf("mock connection ts = %d", ts);
  ApplyToken();
}

void MockConnectionManager::ApplyToken() {
  if (!update_queue_.empty()) {
    GetUpdateResponse()->clear_new_progress_marker();
    sync_pb::DataTypeProgressMarker* new_marker =
        GetUpdateResponse()->add_new_progress_marker();
    new_marker->set_data_type_id(-1);  // Invalid -- clients shouldn't see.
    new_marker->set_token(next_token_);
  }
}

void MockConnectionManager::SetChangesRemaining(int64 timestamp) {
  GetUpdateResponse()->set_changes_remaining(timestamp);
}

void MockConnectionManager::ProcessGetUpdates(ClientToServerMessage* csm,
    ClientToServerResponse* response) {
  CHECK(csm->has_get_updates());
  ASSERT_EQ(csm->message_contents(), ClientToServerMessage::GET_UPDATES);
  const GetUpdatesMessage& gu = csm->get_updates();
  num_get_updates_requests_++;
  EXPECT_FALSE(gu.has_from_timestamp());
  EXPECT_FALSE(gu.has_requested_types());

  if (fail_non_periodic_get_updates_) {
    EXPECT_EQ(sync_pb::GetUpdatesCallerInfo::PERIODIC,
              gu.caller_info().source());
  }

  // Verify that the GetUpdates filter sent by the Syncer matches the test
  // expectation.
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    ModelType model_type = syncable::ModelTypeFromInt(i);
    EXPECT_EQ(expected_filter_[i],
      IsModelTypePresentInSpecifics(gu.from_progress_marker(), model_type))
        << "Syncer requested_types differs from test expectation.";
  }

  // Verify that the items we're about to send back to the client are of
  // the types requested by the client.  If this fails, it probably indicates
  // a test bug.
  EXPECT_TRUE(gu.fetch_folders());
  EXPECT_FALSE(gu.has_requested_types());
  if (update_queue_.empty()) {
    GetUpdateResponse();
  }
  sync_pb::GetUpdatesResponse* updates = &update_queue_.front();
  for (int i = 0; i < updates->entries_size(); ++i) {
    if (!updates->entries(i).deleted()) {
      ModelType entry_type = syncable::GetModelType(updates->entries(i));
      EXPECT_TRUE(
        IsModelTypePresentInSpecifics(gu.from_progress_marker(), entry_type))
          << "Syncer did not request updates being provided by the test.";
    }
  }

  response->mutable_get_updates()->CopyFrom(*updates);

  // Set appropriate progress markers, overriding the value squirreled
  // away by ApplyToken().
  std::string token = response->get_updates().new_progress_marker(0).token();
  response->mutable_get_updates()->clear_new_progress_marker();
  for (int i = 0; i < gu.from_progress_marker_size(); ++i) {
    if (gu.from_progress_marker(i).token() != token) {
      sync_pb::DataTypeProgressMarker* new_marker =
          response->mutable_get_updates()->add_new_progress_marker();
      new_marker->set_data_type_id(gu.from_progress_marker(i).data_type_id());
      new_marker->set_token(token);
    }
  }

  update_queue_.pop_front();
}

void MockConnectionManager::SetClearUserDataResponseStatus(
  sync_pb::ClientToServerResponse::ErrorType errortype ) {
  // Note: this is not a thread-safe set, ok for now.  NOT ok if tests
  // run the syncer on the background thread while this method is called.
  clear_user_data_response_errortype_ = errortype;
}

void MockConnectionManager::ProcessClearData(ClientToServerMessage* csm,
  ClientToServerResponse* response) {
  CHECK(csm->has_clear_user_data());
  ASSERT_EQ(csm->message_contents(), ClientToServerMessage::CLEAR_DATA);
  response->clear_user_data();
  response->set_error_code(clear_user_data_response_errortype_);
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
  commit_messages_->push_back(new CommitMessage);
  commit_messages_->back()->CopyFrom(commit_message);
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
  commit_responses_->push_back(new CommitResponse(*commit_response));
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

SyncEntity* MockConnectionManager::GetMutableLastUpdate() {
  sync_pb::GetUpdatesResponse* updates = GetUpdateResponse();
  EXPECT_TRUE(updates->entries_size() > 0);
  return updates->mutable_entries()->Mutable(updates->entries_size() - 1);
}

void MockConnectionManager::NextUpdateBatch() {
  update_queue_.push_back(sync_pb::GetUpdatesResponse::default_instance());
  SetChangesRemaining(0);
  ApplyToken();
}

const CommitMessage& MockConnectionManager::last_sent_commit() const {
  EXPECT_TRUE(!commit_messages_.empty());
  return *commit_messages_->back();
}

const CommitResponse& MockConnectionManager::last_commit_response() const {
  EXPECT_TRUE(!commit_responses_.empty());
  return *commit_responses_->back();
}

void MockConnectionManager::ThrottleNextRequest(
    ResponseCodeOverrideRequestor* visitor) {
  base::AutoLock lock(response_code_override_lock_);
  throttling_ = true;
  if (visitor)
    visitor->OnOverrideComplete();
}

void MockConnectionManager::FailWithAuthInvalid(
    ResponseCodeOverrideRequestor* visitor) {
  base::AutoLock lock(response_code_override_lock_);
  fail_with_auth_invalid_ = true;
  if (visitor)
    visitor->OnOverrideComplete();
}

void MockConnectionManager::StopFailingWithAuthInvalid(
    ResponseCodeOverrideRequestor* visitor) {
  base::AutoLock lock(response_code_override_lock_);
  fail_with_auth_invalid_ = false;
  if (visitor)
    visitor->OnOverrideComplete();
}

bool MockConnectionManager::IsModelTypePresentInSpecifics(
    const google::protobuf::RepeatedPtrField<
        sync_pb::DataTypeProgressMarker>& filter,
    syncable::ModelType value) {
  int data_type_id = syncable::GetExtensionFieldNumberFromModelType(value);
  for (int i = 0; i < filter.size(); ++i) {
    if (filter.Get(i).data_type_id() == data_type_id) {
      return true;
    }
  }
  return false;
}

void MockConnectionManager::SetServerReachable() {
  server_status_ = HttpResponse::SERVER_CONNECTION_OK;
  server_reachable_ = true;
  browser_sync::ServerConnectionEvent event = {
    browser_sync::ServerConnectionEvent::STATUS_CHANGED,
    server_status_,
    server_reachable_ };
  channel_->NotifyListeners(event);
}

void MockConnectionManager::SetServerNotReachable() {
  server_status_ = HttpResponse::CONNECTION_UNAVAILABLE;
  server_reachable_ = false;
  browser_sync::ServerConnectionEvent event = {
    browser_sync::ServerConnectionEvent::STATUS_CHANGED,
    server_status_,
    server_reachable_ };
  channel_->NotifyListeners(event);
}
