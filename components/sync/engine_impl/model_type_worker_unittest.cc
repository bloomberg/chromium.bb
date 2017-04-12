// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/model_type_worker.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/fake_encryptor.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/engine/model_type_processor.h"
#include "components/sync/engine_impl/commit_contribution.h"
#include "components/sync/engine_impl/cycle/non_blocking_type_debug_info_emitter.h"
#include "components/sync/engine_impl/cycle/status_controller.h"
#include "components/sync/test/engine/mock_model_type_processor.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "components/sync/test/engine/single_type_mock_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Special constant value taken from cryptographer.cc.
const char kNigoriKeyName[] = "nigori-key";

const ModelType kModelType = PREFERENCES;

std::string GenerateTagHash(const std::string& tag) {
  return GenerateSyncableHash(kModelType, tag);
}

const char kTag1[] = "tag1";
const char kTag2[] = "tag2";
const char kTag3[] = "tag3";
const char kValue1[] = "value1";
const char kValue2[] = "value2";
const char kValue3[] = "value3";
const std::string kHash1(GenerateTagHash(kTag1));
const std::string kHash2(GenerateTagHash(kTag2));
const std::string kHash3(GenerateTagHash(kTag3));

sync_pb::EntitySpecifics GenerateSpecifics(const std::string& tag,
                                           const std::string& value) {
  sync_pb::EntitySpecifics specifics;
  specifics.mutable_preference()->set_name(tag);
  specifics.mutable_preference()->set_value(value);
  return specifics;
}

// Returns the name for the given Nigori.
//
// Uses some 'white-box' knowledge to mimic the names that a real sync client
// would generate. It's probably not necessary to do so, but it can't hurt.
std::string GetNigoriName(const Nigori& nigori) {
  std::string name;
  if (!nigori.Permute(Nigori::Password, kNigoriKeyName, &name)) {
    NOTREACHED();
    return std::string();
  }
  return name;
}

// Returns a set of KeyParams for the cryptographer. Each input 'n' value
// results in a different set of parameters.
KeyParams GetNthKeyParams(int n) {
  KeyParams params;
  params.hostname = std::string("localhost");
  params.username = std::string("userX");
  params.password = base::StringPrintf("pw%02d", n);
  return params;
}

// Modifies the input/output parameter |specifics| by encrypting it with
// a Nigori intialized with the specified KeyParams.
void EncryptUpdate(const KeyParams& params,
                   sync_pb::EntitySpecifics* specifics) {
  Nigori nigori;
  nigori.InitByDerivation(params.hostname, params.username, params.password);

  sync_pb::EntitySpecifics original_specifics = *specifics;
  std::string plaintext;
  original_specifics.SerializeToString(&plaintext);

  std::string encrypted;
  nigori.Encrypt(plaintext, &encrypted);

  specifics->Clear();
  AddDefaultFieldValue(kModelType, specifics);
  specifics->mutable_encrypted()->set_key_name(GetNigoriName(nigori));
  specifics->mutable_encrypted()->set_blob(encrypted);
}

}  // namespace

// Tests the ModelTypeWorker.
//
// This class passes messages between the model thread and sync server.
// As such, its code is subject to lots of different race conditions. This
// test harness lets us exhaustively test all possible races. We try to
// focus on just a few interesting cases.
//
// Inputs:
// - Initial data type state from the model thread.
// - Commit requests from the model thread.
// - Update responses from the server.
// - Commit responses from the server.
// - The cryptographer, if encryption is enabled.
//
// Outputs:
// - Commit requests to the server.
// - Commit responses to the model thread.
// - Update responses to the model thread.
// - Nudges to the sync scheduler.
//
// We use the MockModelTypeProcessor to stub out all communication
// with the model thread. That interface is synchronous, which makes it
// much easier to test races.
//
// The interface with the server is built around "pulling" data from this
// class, so we don't have to mock out any of it. We wrap it with some
// convenience functions so we can emulate server behavior.
class ModelTypeWorkerTest : public ::testing::Test {
 public:
  ModelTypeWorkerTest()
      : foreign_encryption_key_index_(0),
        update_encryption_filter_index_(0),
        mock_type_processor_(nullptr),
        mock_server_(kModelType),
        is_processor_disconnected_(false),
        preferences_emitter_(kModelType, &type_observers_) {}

  ~ModelTypeWorkerTest() override {}

  // One of these Initialize functions should be called at the beginning of
  // each test.

  // Initializes with no data type state. We will be unable to perform any
  // significant server action until we receive an update response that
  // contains the type root node for this type.
  void FirstInitialize() {
    sync_pb::ModelTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(kModelType));

    InitializeWithState(initial_state, UpdateResponseDataList());
  }

  // Initializes with some existing data type state. Allows us to start
  // committing items right away.
  void NormalInitialize() {
    InitializeWithPendingUpdates(UpdateResponseDataList());
  }

  // Initialize with some saved pending updates from the model thread.
  void InitializeWithPendingUpdates(
      const UpdateResponseDataList& initial_pending_updates) {
    sync_pb::ModelTypeState initial_state;
    initial_state.mutable_progress_marker()->set_data_type_id(
        GetSpecificsFieldNumberFromModelType(kModelType));
    initial_state.mutable_progress_marker()->set_token(
        "some_saved_progress_token");

    initial_state.set_initial_sync_done(true);

    InitializeWithState(initial_state, initial_pending_updates);

    mock_nudge_handler_.ClearCounters();
  }

  // Initialize with a custom initial ModelTypeState and pending updates.
  void InitializeWithState(
      const sync_pb::ModelTypeState& state,
      const UpdateResponseDataList& initial_pending_updates) {
    DCHECK(!worker_);

    // We don't get to own this object. The |worker_| keeps a unique_ptr to it.
    auto processor = base::MakeUnique<MockModelTypeProcessor>();
    mock_type_processor_ = processor.get();
    processor->SetDisconnectCallback(base::Bind(
        &ModelTypeWorkerTest::DisconnectProcessor, base::Unretained(this)));

    std::unique_ptr<Cryptographer> cryptographer_copy;
    if (cryptographer_) {
      cryptographer_copy = base::MakeUnique<Cryptographer>(*cryptographer_);
    }

    // TODO(maxbogue): crbug.com/529498: Inject pending updates somehow.
    worker_ = base::MakeUnique<ModelTypeWorker>(
        kModelType, state, !state.initial_sync_done(),
        std::move(cryptographer_copy), &mock_nudge_handler_,
        std::move(processor), &preferences_emitter_);
  }

  // Introduce a new key that the local cryptographer can't decrypt.
  void AddPendingKey() {
    if (!cryptographer_) {
      cryptographer_ = base::MakeUnique<Cryptographer>(&fake_encryptor_);
    }

    foreign_encryption_key_index_++;

    sync_pb::NigoriKeyBag bag;

    for (int i = 0; i <= foreign_encryption_key_index_; ++i) {
      Nigori nigori;
      KeyParams params = GetNthKeyParams(i);
      nigori.InitByDerivation(params.hostname, params.username,
                              params.password);

      sync_pb::NigoriKey* key = bag.add_key();

      key->set_name(GetNigoriName(nigori));
      nigori.ExportKeys(key->mutable_user_key(), key->mutable_encryption_key(),
                        key->mutable_mac_key());
    }

    // Re-create the last nigori from that loop.
    Nigori last_nigori;
    KeyParams params = GetNthKeyParams(foreign_encryption_key_index_);
    last_nigori.InitByDerivation(params.hostname, params.username,
                                 params.password);

    // Serialize and encrypt the bag with the last nigori.
    std::string serialized_bag;
    bag.SerializeToString(&serialized_bag);

    sync_pb::EncryptedData encrypted;
    encrypted.set_key_name(GetNigoriName(last_nigori));
    last_nigori.Encrypt(serialized_bag, encrypted.mutable_blob());

    // Update the cryptographer with new pending keys.
    cryptographer_->SetPendingKeys(encrypted);

    // Update the worker with the latest cryptographer.
    if (worker_) {
      worker_->UpdateCryptographer(
          base::MakeUnique<Cryptographer>(*cryptographer_));
    }
  }

  // Update the local cryptographer with all relevant keys.
  void DecryptPendingKey() {
    if (!cryptographer_) {
      cryptographer_ = base::MakeUnique<Cryptographer>(&fake_encryptor_);
    }

    KeyParams params = GetNthKeyParams(foreign_encryption_key_index_);
    bool success = cryptographer_->DecryptPendingKeys(params);
    DCHECK(success);

    // Update the worker with the latest cryptographer.
    if (worker_) {
      worker_->UpdateCryptographer(
          base::MakeUnique<Cryptographer>(*cryptographer_));
    }
  }

  // Use the Nth nigori instance to encrypt incoming updates.
  // The default value, zero, indicates no encryption.
  void SetUpdateEncryptionFilter(int n) { update_encryption_filter_index_ = n; }

  // Modifications on the model thread that get sent to the worker under test.

  void CommitRequest(const std::string& name, const std::string& value) {
    const std::string tag_hash = GenerateTagHash(name);
    CommitRequestData data = mock_type_processor_->CommitRequest(
        tag_hash, GenerateSpecifics(name, value));
    worker_->EnqueueForCommit({data});
  }

  void DeleteRequest(const std::string& tag) {
    const std::string tag_hash = GenerateTagHash(tag);
    CommitRequestData data = mock_type_processor_->DeleteRequest(tag_hash);
    worker_->EnqueueForCommit({data});
  }

  // Pretend to receive update messages from the server.

  void TriggerTypeRootUpdateFromServer() {
    sync_pb::SyncEntity entity = mock_server_.TypeRootUpdate();
    worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                       mock_server_.GetContext(), {&entity},
                                       nullptr);
    worker_->PassiveApplyUpdates(nullptr);
  }

  void TriggerPartialUpdateFromServer(int64_t version_offset,
                                      const std::string& tag,
                                      const std::string& value) {
    sync_pb::SyncEntity entity = mock_server_.UpdateFromServer(
        version_offset, GenerateTagHash(tag), GenerateSpecifics(tag, value));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity.mutable_specifics());
    }

    worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                       mock_server_.GetContext(), {&entity},
                                       nullptr);
  }

  void TriggerUpdateFromServer(int64_t version_offset,
                               const std::string& tag,
                               const std::string& value) {
    TriggerPartialUpdateFromServer(version_offset, tag, value);
    worker_->ApplyUpdates(nullptr);
  }

  void TriggerTombstoneFromServer(int64_t version_offset,
                                  const std::string& tag) {
    sync_pb::SyncEntity entity =
        mock_server_.TombstoneFromServer(version_offset, GenerateTagHash(tag));

    if (update_encryption_filter_index_ != 0) {
      EncryptUpdate(GetNthKeyParams(update_encryption_filter_index_),
                    entity.mutable_specifics());
    }

    worker_->ProcessGetUpdatesResponse(mock_server_.GetProgress(),
                                       mock_server_.GetContext(), {&entity},
                                       nullptr);
    worker_->ApplyUpdates(nullptr);
  }

  // Simulates the end of a GU sync cycle and tells the worker to flush changes
  // to the processor.
  void ApplyUpdates() { worker_->ApplyUpdates(nullptr); }

  // Delivers specified protos as updates.
  //
  // Does not update mock server state. Should be used as a last resort when
  // writing test cases that require entities that don't fit the normal sync
  // protocol. Try to use the other, higher level methods if possible.
  void DeliverRawUpdates(const SyncEntityList& list) {
    worker_->ProcessGetUpdatesResponse(
        mock_server_.GetProgress(), mock_server_.GetContext(), list, nullptr);
    worker_->ApplyUpdates(nullptr);
  }

  // By default, this harness behaves as if all tasks posted to the model
  // thread are executed immediately. However, this is not necessarily true.
  // The model's TaskRunner has a queue, and the tasks we post to it could
  // linger there for a while. In the meantime, the model thread could
  // continue posting tasks to the worker based on its stale state.
  //
  // If you want to test those race cases, then these functions are for you.
  void SetModelThreadIsSynchronous(bool is_synchronous) {
    mock_type_processor_->SetSynchronousExecution(is_synchronous);
  }
  void PumpModelThread() { mock_type_processor_->RunQueuedTasks(); }

  // Returns true if the |worker_| is ready to commit something.
  bool WillCommit() {
    std::unique_ptr<CommitContribution> contribution(
        worker_->GetContribution(INT_MAX));

    if (contribution) {
      contribution->CleanUp();  // Gracefully abort the commit.
      return true;
    } else {
      return false;
    }
  }

  // Pretend to successfully commit all outstanding unsynced items.
  // It is safe to call this only if WillCommit() returns true.
  // Conveniently, this is all one big synchronous operation. The sync thread
  // remains blocked while the commit is in progress, so we don't need to worry
  // about other tasks being run between the time when the commit request is
  // issued and the time when the commit response is received.
  void DoSuccessfulCommit() {
    DCHECK(WillCommit());
    std::unique_ptr<CommitContribution> contribution(
        worker_->GetContribution(INT_MAX));

    sync_pb::ClientToServerMessage message;
    contribution->AddToCommitMessage(&message);

    sync_pb::ClientToServerResponse response =
        mock_server_.DoSuccessfulCommit(message);

    contribution->ProcessCommitResponse(response, nullptr);
    contribution->CleanUp();
  }

  // Callback when processor got disconnected with sync.
  void DisconnectProcessor() {
    DCHECK(!is_processor_disconnected_);
    is_processor_disconnected_ = true;
  }

  bool IsProcessorDisconnected() { return is_processor_disconnected_; }

  void ResetWorker() { worker_.reset(); }

  // Returns the number of commit nudges sent to the mock nudge handler.
  int GetNumCommitNudges() const {
    return mock_nudge_handler_.GetNumCommitNudges();
  }

  // Returns the number of initial sync nudges sent to the mock nudge handler.
  int GetNumInitialDownloadNudges() const {
    return mock_nudge_handler_.GetNumInitialDownloadNudges();
  }

  // Returns the name of the encryption key in the cryptographer last passed to
  // the CommitQueue. Returns an empty string if no crypgorapher is
  // in use. See also: DecryptPendingKey().
  std::string GetLocalCryptographerKeyName() const {
    if (!cryptographer_) {
      return std::string();
    }
    return cryptographer_->GetDefaultNigoriKeyName();
  }

  MockModelTypeProcessor* processor() { return mock_type_processor_; }
  ModelTypeWorker* worker() { return worker_.get(); }
  SingleTypeMockServer* server() { return &mock_server_; }
  NonBlockingTypeDebugInfoEmitter* emitter() { return &preferences_emitter_; }

 private:
  // An encryptor for our cryptographer.
  FakeEncryptor fake_encryptor_;

  // The cryptographer itself. Null if we're not encrypting the type.
  std::unique_ptr<Cryptographer> cryptographer_;

  // The number of the most recent foreign encryption key known to our
  // cryptographer. Note that not all of these will be decryptable.
  int foreign_encryption_key_index_;

  // The number of the encryption key used to encrypt incoming updates. A zero
  // value implies no encryption.
  int update_encryption_filter_index_;

  // The ModelTypeWorker being tested.
  std::unique_ptr<ModelTypeWorker> worker_;

  // Non-owned, possibly null pointer. This object belongs to the
  // ModelTypeWorker under test.
  MockModelTypeProcessor* mock_type_processor_;

  // A mock that emulates enough of the sync server that it can be used
  // a single UpdateHandler and CommitContributor pair. In this test
  // harness, the |worker_| is both of them.
  SingleTypeMockServer mock_server_;

  // A mock to track the number of times the CommitQueue requests to
  // sync.
  MockNudgeHandler mock_nudge_handler_;

  bool is_processor_disconnected_;

  base::ObserverList<TypeDebugInfoObserver> type_observers_;

  NonBlockingTypeDebugInfoEmitter preferences_emitter_;
};

// Requests a commit and verifies the messages sent to the client and server as
// a result.
//
// This test performs sanity checks on most of the fields in these messages.
// For the most part this is checking that the test code behaves as expected
// and the |worker_| doesn't mess up its simple task of moving around these
// values. It makes sense to have one or two tests that are this thorough, but
// we shouldn't be this verbose in all tests.
TEST_F(ModelTypeWorkerTest, SimpleCommit) {
  NormalInitialize();

  EXPECT_FALSE(WillCommit());
  EXPECT_EQ(0U, server()->GetNumCommitMessages());
  EXPECT_EQ(0U, processor()->GetNumCommitResponses());
  EXPECT_EQ(0, emitter()->GetCommitCounters().num_commits_attempted);
  EXPECT_EQ(0, emitter()->GetCommitCounters().num_commits_success);

  CommitRequest(kTag1, kValue1);

  EXPECT_EQ(1, GetNumCommitNudges());

  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  const std::string& client_tag_hash = GenerateTagHash(kTag1);

  // Exhaustively verify the SyncEntity sent in the commit message.
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const sync_pb::SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(0, entity.version());
  EXPECT_NE(0, entity.mtime());
  EXPECT_NE(0, entity.ctime());
  EXPECT_FALSE(entity.name().empty());
  EXPECT_EQ(client_tag_hash, entity.client_defined_unique_tag());
  EXPECT_EQ(kTag1, entity.specifics().preference().name());
  EXPECT_FALSE(entity.deleted());
  EXPECT_EQ(kValue1, entity.specifics().preference().value());

  // Verify the counters update correctly.
  EXPECT_EQ(1, emitter()->GetCommitCounters().num_commits_attempted);
  EXPECT_EQ(1, emitter()->GetCommitCounters().num_commits_success);

  // Exhaustively verify the commit response returned to the model thread.
  ASSERT_EQ(1U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(0).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);

  // The ID changes in a commit response to initial commit.
  EXPECT_FALSE(commit_response.id.empty());
  EXPECT_NE(entity.id_string(), commit_response.id);

  EXPECT_EQ(client_tag_hash, commit_response.client_tag_hash);
  EXPECT_LT(0, commit_response.response_version);
  EXPECT_LT(0, commit_response.sequence_number);
  EXPECT_FALSE(commit_response.specifics_hash.empty());
}

TEST_F(ModelTypeWorkerTest, SimpleDelete) {
  NormalInitialize();

  // We can't delete an entity that was never committed.
  // Step 1 is to create and commit a new entity.
  CommitRequest(kTag1, kValue1);
  EXPECT_EQ(1, GetNumCommitNudges());
  EXPECT_EQ(0, emitter()->GetCommitCounters().num_commits_attempted);
  EXPECT_EQ(0, emitter()->GetCommitCounters().num_commits_success);
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  // Verify the counters update correctly.
  EXPECT_EQ(1, emitter()->GetCommitCounters().num_commits_attempted);
  EXPECT_EQ(1, emitter()->GetCommitCounters().num_commits_success);

  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& initial_commit_response =
      processor()->GetCommitResponse(kHash1);
  int64_t base_version = initial_commit_response.response_version;

  // Now that we have an entity, we can delete it.
  DeleteRequest(kTag1);
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  // Verify the counters update correctly.
  EXPECT_EQ(2, emitter()->GetCommitCounters().num_commits_attempted);
  EXPECT_EQ(2, emitter()->GetCommitCounters().num_commits_success);

  // Verify the SyncEntity sent in the commit message.
  ASSERT_EQ(2U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(1).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const sync_pb::SyncEntity& entity = server()->GetLastCommittedEntity(kHash1);
  EXPECT_FALSE(entity.id_string().empty());
  EXPECT_EQ(GenerateTagHash(kTag1), entity.client_defined_unique_tag());
  EXPECT_EQ(base_version, entity.version());
  EXPECT_TRUE(entity.deleted());

  // Deletions should contain enough specifics to identify the type.
  EXPECT_TRUE(entity.has_specifics());
  EXPECT_EQ(kModelType, GetModelTypeFromSpecifics(entity.specifics()));

  // Verify the commit response returned to the model thread.
  ASSERT_EQ(2U, processor()->GetNumCommitResponses());
  EXPECT_EQ(1U, processor()->GetNthCommitResponse(1).size());
  ASSERT_TRUE(processor()->HasCommitResponse(kHash1));
  const CommitResponseData& commit_response =
      processor()->GetCommitResponse(kHash1);

  EXPECT_EQ(entity.id_string(), commit_response.id);
  EXPECT_EQ(entity.client_defined_unique_tag(),
            commit_response.client_tag_hash);
  EXPECT_EQ(entity.version(), commit_response.response_version);
}

// The server doesn't like it when we try to delete an entity it's never heard
// of before. This test helps ensure we avoid that scenario.
TEST_F(ModelTypeWorkerTest, NoDeleteUncommitted) {
  NormalInitialize();

  // Request the commit of a new, never-before-seen item.
  CommitRequest(kTag1, kValue1);
  EXPECT_TRUE(WillCommit());
  EXPECT_EQ(1, GetNumCommitNudges());

  // Request a deletion of that item before we've had a chance to commit it.
  DeleteRequest(kTag1);
  EXPECT_FALSE(WillCommit());
  EXPECT_EQ(2, GetNumCommitNudges());
}

// Verifies the sending of an "initial sync done" signal.
TEST_F(ModelTypeWorkerTest, SendInitialSyncDone) {
  FirstInitialize();  // Initialize with no saved sync state.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(1, GetNumInitialDownloadNudges());

  EXPECT_FALSE(worker()->IsInitialSyncEnded());

  // Receive an update response that contains only the type root node.
  TriggerTypeRootUpdateFromServer();

  // One update triggered by ApplyUpdates, which the worker interprets to mean
  // "initial sync done". This triggers a model thread update, too.
  EXPECT_EQ(1U, processor()->GetNumUpdateResponses());

  // The update contains no entities.
  EXPECT_EQ(0U, processor()->GetNthUpdateResponse(0).size());

  const sync_pb::ModelTypeState& state = processor()->GetNthUpdateState(0);
  EXPECT_FALSE(state.progress_marker().token().empty());
  EXPECT_TRUE(state.initial_sync_done());
  EXPECT_TRUE(worker()->IsInitialSyncEnded());
}

// Commit two new entities in two separate commit messages.
TEST_F(ModelTypeWorkerTest, TwoNewItemsCommittedSeparately) {
  NormalInitialize();

  // Commit the first of two entities.
  CommitRequest(kTag1, kValue1);
  EXPECT_EQ(1, GetNumCommitNudges());
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const sync_pb::SyncEntity& tag1_entity =
      server()->GetLastCommittedEntity(kHash1);

  // Commit the second of two entities.
  CommitRequest(kTag2, kValue2);
  EXPECT_EQ(2, GetNumCommitNudges());
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  ASSERT_EQ(2U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(1).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash2));
  const sync_pb::SyncEntity& tag2_entity =
      server()->GetLastCommittedEntity(kHash2);

  EXPECT_FALSE(WillCommit());

  // The IDs assigned by the |worker_| should be unique.
  EXPECT_NE(tag1_entity.id_string(), tag2_entity.id_string());

  // Check that the committed specifics values are sane.
  EXPECT_EQ(tag1_entity.specifics().preference().value(), kValue1);
  EXPECT_EQ(tag2_entity.specifics().preference().value(), kValue2);

  // There should have been two separate commit responses sent to the model
  // thread. They should be uninteresting, so we don't bother inspecting them.
  EXPECT_EQ(2U, processor()->GetNumCommitResponses());
}

// Test normal update receipt code path.
TEST_F(ModelTypeWorkerTest, ReceiveUpdates) {
  NormalInitialize();

  EXPECT_EQ(0, emitter()->GetUpdateCounters().num_updates_received);
  EXPECT_EQ(0, emitter()->GetUpdateCounters().num_updates_applied);

  const std::string& tag_hash = GenerateTagHash(kTag1);

  TriggerUpdateFromServer(10, kTag1, kValue1);

  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  UpdateResponseDataList updates_list = processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(1U, updates_list.size());

  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  UpdateResponseData update = processor()->GetUpdateResponse(kHash1);
  const EntityData& entity = update.entity.value();

  EXPECT_FALSE(entity.id.empty());
  EXPECT_EQ(tag_hash, entity.client_tag_hash);
  EXPECT_LT(0, update.response_version);
  EXPECT_FALSE(entity.creation_time.is_null());
  EXPECT_FALSE(entity.modification_time.is_null());
  EXPECT_FALSE(entity.non_unique_name.empty());
  EXPECT_FALSE(entity.is_deleted());
  EXPECT_EQ(kTag1, entity.specifics.preference().name());
  EXPECT_EQ(kValue1, entity.specifics.preference().value());

  // Verify the counters update correctly.
  EXPECT_EQ(1, emitter()->GetUpdateCounters().num_updates_received);
  EXPECT_EQ(1, emitter()->GetUpdateCounters().num_updates_applied);
}

// Test that an update download coming in multiple parts gets accumulated into
// one call to the processor.
TEST_F(ModelTypeWorkerTest, ReceiveMultiPartUpdates) {
  NormalInitialize();

  // A partial update response doesn't pass anything to the processor.
  TriggerPartialUpdateFromServer(10, kTag1, kValue1);
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Trigger the completion of the update.
  TriggerUpdateFromServer(10, kTag2, kValue2);

  // Processor received exactly one update with entities in the right order.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  UpdateResponseDataList updates = processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(2U, updates.size());
  EXPECT_EQ(GenerateTagHash(kTag1), updates[0].entity->client_tag_hash);
  EXPECT_EQ(GenerateTagHash(kTag2), updates[1].entity->client_tag_hash);

  // A subsequent update doesn't pass the same entities again.
  TriggerUpdateFromServer(10, kTag3, kValue3);
  ASSERT_EQ(2U, processor()->GetNumUpdateResponses());
  updates = processor()->GetNthUpdateResponse(1);
  ASSERT_EQ(1U, updates.size());
  EXPECT_EQ(GenerateTagHash(kTag3), updates[0].entity->client_tag_hash);
}

// Test that updates with no entities behave correctly.
TEST_F(ModelTypeWorkerTest, EmptyUpdates) {
  NormalInitialize();

  server()->SetProgressMarkerToken("token2");
  DeliverRawUpdates(SyncEntityList());
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  ASSERT_EQ(
      server()->GetProgress().SerializeAsString(),
      processor()->GetNthUpdateState(0).progress_marker().SerializeAsString());
}

// Test commit of encrypted updates.
TEST_F(ModelTypeWorkerTest, EncryptedCommit) {
  NormalInitialize();

  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Tell the worker about the new encryption key but no ApplyUpdates yet.
  AddPendingKey();
  DecryptPendingKey();
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Now the information is allowed to reach the processor.
  ApplyUpdates();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());

  // Normal commit request stuff.
  CommitRequest(kTag1, kValue1);
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const sync_pb::SyncEntity& tag1_entity =
      server()->GetLastCommittedEntity(kHash1);

  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());

  // The title should be overwritten.
  EXPECT_EQ(tag1_entity.name(), "encrypted");

  // The type should be set, but there should be no non-encrypted contents.
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test that updates are not delivered to the processor when encryption is
// required but unavailable.
TEST_F(ModelTypeWorkerTest, EncryptionBlocksUpdates) {
  NormalInitialize();

  // Update encryption key name, should be blocked.
  AddPendingKey();
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Update progress marker, should be blocked.
  server()->SetProgressMarkerToken("token2");
  DeliverRawUpdates(SyncEntityList());
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Update local cryptographer, verify everything is pushed to processor.
  DecryptPendingKey();
  ApplyUpdates();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  UpdateResponseDataList updates_list = processor()->GetNthUpdateResponse(0);
  ASSERT_EQ(
      server()->GetProgress().SerializeAsString(),
      processor()->GetNthUpdateState(0).progress_marker().SerializeAsString());
}

// Test items are not committed when encryption is required but unavailable.
TEST_F(ModelTypeWorkerTest, EncryptionBlocksCommits) {
  NormalInitialize();

  CommitRequest(kTag1, kValue1);
  EXPECT_TRUE(WillCommit());

  // We know encryption is in use on this account, but don't have the necessary
  // encryption keys. The worker should refuse to commit.
  AddPendingKey();
  EXPECT_FALSE(WillCommit());

  // Once the cryptographer is returned to a normal state, we should be able to
  // commit again.
  DecryptPendingKey();
  EXPECT_TRUE(WillCommit());

  // Verify the committed entity was properly encrypted.
  DoSuccessfulCommit();
  ASSERT_EQ(1U, server()->GetNumCommitMessages());
  EXPECT_EQ(1, server()->GetNthCommitMessage(0).commit().entries_size());
  ASSERT_TRUE(server()->HasCommitEntity(kHash1));
  const sync_pb::SyncEntity& tag1_entity =
      server()->GetLastCommittedEntity(kHash1);
  EXPECT_TRUE(tag1_entity.specifics().has_encrypted());
  EXPECT_EQ(tag1_entity.name(), "encrypted");
  EXPECT_TRUE(tag1_entity.specifics().has_preference());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_name());
  EXPECT_FALSE(tag1_entity.specifics().preference().has_value());
}

// Test the receipt of decryptable entities.
TEST_F(ModelTypeWorkerTest, ReceiveDecryptableEntities) {
  NormalInitialize();

  // Create a new Nigori and allow the cryptographer to decrypt it.
  AddPendingKey();
  DecryptPendingKey();

  // First, receive an unencrypted entry.
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // Test some basic properties regarding the update.
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  UpdateResponseData update1 = processor()->GetUpdateResponse(kHash1);
  EXPECT_EQ(kTag1, update1.entity->specifics.preference().name());
  EXPECT_EQ(kValue1, update1.entity->specifics.preference().value());
  EXPECT_TRUE(update1.encryption_key_name.empty());

  // Set received updates to be encrypted using the new nigori.
  SetUpdateEncryptionFilter(1);

  // This next update will be encrypted.
  TriggerUpdateFromServer(10, kTag2, kValue2);

  // Test its basic features and the value of encryption_key_name.
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash2));
  UpdateResponseData update2 = processor()->GetUpdateResponse(kHash2);
  EXPECT_EQ(kTag2, update2.entity->specifics.preference().name());
  EXPECT_EQ(kValue2, update2.entity->specifics.preference().value());
  EXPECT_FALSE(update2.encryption_key_name.empty());
}

// Test initializing a CommitQueue with a cryptographer at startup.
TEST_F(ModelTypeWorkerTest, InitializeWithCryptographer) {
  // Set up some encryption state.
  AddPendingKey();
  DecryptPendingKey();

  // Then initialize.
  NormalInitialize();

  // The worker should tell the model thread about encryption as soon as
  // possible, so that it will have the chance to re-encrypt local data if
  // necessary.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initialzing with a cryptographer that is not ready.
TEST_F(ModelTypeWorkerTest, InitializeWithPendingCryptographer) {
  // Only add a pending key, cryptographer will not be ready.
  AddPendingKey();

  // Then initialize.
  NormalInitialize();

  // Shouldn't be informed of the EKN, since there's a pending key.
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Although the cryptographer is ready, it should wait until being told to
  // update the processor.
  DecryptPendingKey();
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  ApplyUpdates();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Test initializing with a cryptographer on first startup.
TEST_F(ModelTypeWorkerTest, FirstInitializeWithCryptographer) {
  // Set up a Cryptographer that's good to go.
  AddPendingKey();
  DecryptPendingKey();

  // Initialize with initial sync done to false.
  FirstInitialize();

  // Shouldn't be informed of the EKN, since normal activation will drive this.
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // Now perform first sync and make sure the EKN makes it.
  TriggerTypeRootUpdateFromServer();
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  EXPECT_EQ(GetLocalCryptographerKeyName(),
            processor()->GetNthUpdateState(0).encryption_key_name());
}

// Receive updates that are initially undecryptable, then ensure they get
// delivered to the model thread upon ApplyUpdates() after decryption becomes
// possible.
TEST_F(ModelTypeWorkerTest, ReceiveUndecryptableEntries) {
  NormalInitialize();

  // Receive a new foreign encryption key that we can't decrypt.
  AddPendingKey();

  // Receive an encrypted with that new key, which we can't access.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);

  // At this point, the cryptographer does not have access to the key, so the
  // updates will be undecryptable. This will block all updates.
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // The update can be delivered once the cryptographer is ready, but it'll
  // still wait for the apply call.
  DecryptPendingKey();
  ASSERT_EQ(0U, processor()->GetNumUpdateResponses());

  // ApplyUpdates() will cause everything to finally be delivered.
  ApplyUpdates();
  ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
  UpdateResponseData update = processor()->GetUpdateResponse(kHash1);
  EXPECT_EQ(kTag1, update.entity->specifics.preference().name());
  EXPECT_EQ(kValue1, update.entity->specifics.preference().value());
  EXPECT_EQ(GetLocalCryptographerKeyName(), update.encryption_key_name);
}

// Ensure that even encrypted updates can cause conflicts.
TEST_F(ModelTypeWorkerTest, EncryptedUpdateOverridesPendingCommit) {
  NormalInitialize();

  // Prepare to commit an item.
  CommitRequest(kTag1, kValue1);
  EXPECT_TRUE(WillCommit());

  // Receive an encrypted update for that item and the new key. The pending key
  // needs to arrive during (or before) the sync cycle that contains the
  // encrypted entity. This means before ApplyUpdates is called.
  SetUpdateEncryptionFilter(1);
  TriggerPartialUpdateFromServer(10, kTag1, kValue1);
  AddPendingKey();
  DecryptPendingKey();
  ApplyUpdates();

  // The pending commit state should be cleared.
  EXPECT_FALSE(WillCommit());

  // The encrypted update will be delivered to the model thread.
  ASSERT_EQ(1U, processor()->GetNumUpdateResponses());
  UpdateResponseDataList updates_list = processor()->GetNthUpdateResponse(0);
  EXPECT_EQ(1U, updates_list.size());
}

// Commit twice, both times with the kUncommittedVersion base version. Then
// verify the second time through that we see the correct version.
TEST_F(ModelTypeWorkerTest, OldVersionCommit) {
  NormalInitialize();
  CommitRequest(kTag1, kValue1);
  EXPECT_TRUE(WillCommit());
  DoSuccessfulCommit();
  int commit_version = processor()->GetCommitResponse(kHash1).response_version;
  EXPECT_NE(kUncommittedVersion, commit_version);

  // Create a custom commit request using the old base_version.
  CommitRequestData data =
      processor()->CommitRequest(kHash1, GenerateSpecifics(kTag1, kValue2));
  data.base_version = kUncommittedVersion;
  worker()->EnqueueForCommit({data});
  EXPECT_TRUE(WillCommit());
  DoSuccessfulCommit();
  sync_pb::ClientToServerMessage message = server()->GetNthCommitMessage(1);
  const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entries =
      message.commit().entries();
  ASSERT_EQ(1, entries.size());
  EXPECT_EQ(entries.Get(0).version(), commit_version);
}

// Test decryption of pending updates saved across a restart.
TEST_F(ModelTypeWorkerTest, RestorePendingEntries) {
  // Create a fake pending update.
  EntityData entity;
  entity.client_tag_hash = GenerateTagHash(kTag1);
  entity.id = "SomeID";
  entity.creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10);
  entity.modification_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(11);
  entity.non_unique_name = "encrypted";
  entity.specifics = GenerateSpecifics(kTag1, kValue1);
  EncryptUpdate(GetNthKeyParams(1), &(entity.specifics));

  UpdateResponseData update;
  update.entity = entity.PassToPtr();
  update.response_version = 100;

  // Inject the update during CommitQueue initialization.
  InitializeWithPendingUpdates({update});

  // Update will be undecryptable at first.
  EXPECT_EQ(0U, processor()->GetNumUpdateResponses());
  ASSERT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Update the cryptographer so it can decrypt that update.
  AddPendingKey();
  DecryptPendingKey();

  // Verify the item gets decrypted and sent back to the model thread.
  // TODO(maxbogue): crbug.com/529498: Uncomment when pending updates are
  // handled by the worker again.
  // ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Test decryption of pending updates saved across a restart. This test
// differs from the previous one in that the restored updates can be decrypted
// immediately after the CommitQueue is constructed.
TEST_F(ModelTypeWorkerTest, RestoreApplicableEntries) {
  // Update the cryptographer so it can decrypt that update.
  AddPendingKey();
  DecryptPendingKey();

  // Create a fake pending update.
  EntityData entity;
  entity.client_tag_hash = GenerateTagHash(kTag1);
  entity.id = "SomeID";
  entity.creation_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10);
  entity.modification_time =
      base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(11);
  entity.non_unique_name = "encrypted";

  entity.specifics = GenerateSpecifics(kTag1, kValue1);
  EncryptUpdate(GetNthKeyParams(1), &(entity.specifics));

  UpdateResponseData update;
  update.entity = entity.PassToPtr();
  update.response_version = 100;

  // Inject the update during CommitQueue initialization.
  InitializeWithPendingUpdates({update});

  // Verify the item gets decrypted and sent back to the model thread.
  // TODO(maxbogue): crbug.com/529498: Uncomment when pending updates are
  // handled by the worker again.
  // ASSERT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Test that undecryptable updates provide sufficient reason to not commit.
//
// This should be rare in practice. Usually the cryptographer will be in an
// unusable state when we receive undecryptable updates, and that alone will be
// enough to prevent all commits.
TEST_F(ModelTypeWorkerTest, CommitBlockedByPending) {
  NormalInitialize();

  // Prepare to commit an item.
  CommitRequest(kTag1, kValue1);
  EXPECT_TRUE(WillCommit());

  // Receive an encrypted update for that item and the new key. The pending key
  // needs to arrive during (or before) the sync cycle that contains the
  // encrypted entity. This means before ApplyUpdates is called.
  SetUpdateEncryptionFilter(1);
  TriggerPartialUpdateFromServer(10, kTag1, kValue1);
  AddPendingKey();
  ApplyUpdates();

  // Update should not have made it to the processor.
  ASSERT_FALSE(processor()->HasUpdateResponse(kHash1));

  // The pending commit state should be cleared.
  EXPECT_FALSE(WillCommit());

  // Pretend the update arrived too late to prevent another commit request.
  CommitRequest(kTag1, kValue2);

  EXPECT_FALSE(WillCommit());
}

// Verify that corrupted encrypted updates don't cause crashes.
TEST_F(ModelTypeWorkerTest, ReceiveCorruptEncryption) {
  // Initialize the worker with basic encryption state.
  NormalInitialize();
  AddPendingKey();
  DecryptPendingKey();

  // Manually create an update.
  sync_pb::SyncEntity entity;
  entity.set_client_defined_unique_tag(GenerateTagHash(kTag1));
  entity.set_id_string("SomeID");
  entity.set_version(1);
  entity.set_ctime(1000);
  entity.set_mtime(1001);
  entity.set_name("encrypted");
  entity.set_deleted(false);

  // Encrypt it.
  entity.mutable_specifics()->CopyFrom(GenerateSpecifics(kTag1, kValue1));
  EncryptUpdate(GetNthKeyParams(1), entity.mutable_specifics());

  // Replace a few bytes to corrupt it.
  entity.mutable_specifics()->mutable_encrypted()->mutable_blob()->replace(
      0, 4, "xyz!");

  // If a corrupt update could trigger a crash, this is where it would happen.
  DeliverRawUpdates({&entity});

  EXPECT_FALSE(processor()->HasUpdateResponse(kHash1));

  // Deliver a non-corrupt update to see if the everything still works.
  SetUpdateEncryptionFilter(1);
  TriggerUpdateFromServer(10, kTag1, kValue1);
  EXPECT_TRUE(processor()->HasUpdateResponse(kHash1));
}

// Test that processor has been disconnected from Sync when worker got
// disconnected.
TEST_F(ModelTypeWorkerTest, DisconnectProcessorFromSyncTest) {
  // Initialize the worker with basic state.
  NormalInitialize();
  EXPECT_FALSE(IsProcessorDisconnected());
  ResetWorker();
  EXPECT_TRUE(IsProcessorDisconnected());
}

// Test that deleted entity can be recreated again.
TEST_F(ModelTypeWorkerTest, RecreateDeletedEntity) {
  NormalInitialize();

  // Create, then delete entity.
  CommitRequest(kTag1, kValue1);
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  DeleteRequest(kTag1);
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();

  // Verify that entity got deleted from the server.
  {
    const sync_pb::SyncEntity& entity =
        server()->GetLastCommittedEntity(kHash1);
    EXPECT_TRUE(entity.deleted());
  }

  // Create the same entity again.
  CommitRequest(kTag1, kValue1);
  ASSERT_TRUE(WillCommit());
  DoSuccessfulCommit();
  // Verify that there is a valid entity on the server.
  {
    const sync_pb::SyncEntity& entity =
        server()->GetLastCommittedEntity(kHash1);
    EXPECT_FALSE(entity.deleted());
  }
}

}  // namespace syncer
