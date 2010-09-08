// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/clear_data_command.h"
#include "chrome/browser/sync/protocol/autofill_specifics.pb.h"
#include "chrome/browser/sync/protocol/bookmark_specifics.pb.h"
#include "chrome/browser/sync/protocol/preference_specifics.pb.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/test/sync/engine/proto_extension_validator.h"
#include "chrome/test/sync/engine/syncer_command_test.h"

namespace browser_sync {

using sessions::ScopedSessionContextSyncerEventChannel;
using syncable::FIRST_REAL_MODEL_TYPE;
using syncable::MODEL_TYPE_COUNT;

// A test fixture for tests exercising ClearDataCommandTest.
class ClearDataCommandTest : public SyncerCommandTest {
 protected:
  ClearDataCommandTest() {}
  ClearDataCommand command_;

  virtual void OnShouldStopSyncingPermanently() {
    on_should_stop_syncing_permanently_called_ = true;
  }

 protected:
  bool on_should_stop_syncing_permanently_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClearDataCommandTest);
};

class ClearEventHandler : public ChannelEventHandler<SyncerEvent> {
 public:
  ClearEventHandler() {
    ResetReceivedEvents();
  }
  bool ReceievedClearSuccessEvent() { return received_clear_success_event_; }
  bool ReceievedClearFailedEvent() { return received_clear_failed_event_; }
  void ResetReceivedEvents() {
    received_clear_success_event_ = false;
    received_clear_failed_event_ = false;
  }

  void HandleChannelEvent(const SyncerEvent& event);

 private:
  bool received_clear_success_event_;
  bool received_clear_failed_event_;
};

void ClearEventHandler::HandleChannelEvent(const SyncerEvent& event) {
  if (event.what_happened == SyncerEvent::CLEAR_SERVER_DATA_FAILED) {
      received_clear_failed_event_ = true;
  } else if (event.what_happened == SyncerEvent::CLEAR_SERVER_DATA_SUCCEEDED) {
      received_clear_success_event_ = true;
  }
}

TEST_F(ClearDataCommandTest, ClearDataCommandExpectFailed) {
  syncable::ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
  ASSERT_TRUE(dir.good());

  ConfigureMockServerConnection();
  scoped_ptr<SyncerEventChannel> channel(new SyncerEventChannel());
  scoped_ptr<ClearEventHandler> handler(new ClearEventHandler());
  scoped_ptr<browser_sync::ChannelHookup<SyncerEvent> > hookup(
      channel.get()->AddObserver(handler.get()));
  ScopedSessionContextSyncerEventChannel s_channel(context(), channel.get());

  dir->set_store_birthday(mock_server()->store_birthday());
  mock_server()->SetClearUserDataResponseStatus(sync_pb::RETRIABLE_ERROR);
  on_should_stop_syncing_permanently_called_ = false;

  command_.Execute(session());

  // Expect that the client sent a clear request, received failure,
  // fired a failure event, but did not disable sync.
  //
  // A failure event will be bubbled back to the user's UI, and the
  // user can press "clear" again.
  //
  // We do not want to disable sync in the client because the user may
  // incorrectly get the impression that their private data has been cleared
  // from the server (from the fact that their data is gone on the client).
  //
  // Any subsequent GetUpdates/Commit requests or attempts to enable sync
  // will cause the server to attempt to resume the clearing process (within
  // a bounded window of time)
  const sync_pb::ClientToServerMessage& r = mock_server()->last_request();
  EXPECT_TRUE(r.has_clear_user_data());

  EXPECT_TRUE(handler.get()->ReceievedClearFailedEvent());

  EXPECT_FALSE(handler.get()->ReceievedClearSuccessEvent());
  EXPECT_FALSE(on_should_stop_syncing_permanently_called_);
}

TEST_F(ClearDataCommandTest, ClearDataCommandExpectSucess) {
  syncable::ScopedDirLookup dir(syncdb()->manager(), syncdb()->name());
  ASSERT_TRUE(dir.good());

  ConfigureMockServerConnection();
  scoped_ptr<SyncerEventChannel> channel(new SyncerEventChannel());
  scoped_ptr<ClearEventHandler> handler(new ClearEventHandler());
  scoped_ptr<browser_sync::ChannelHookup<SyncerEvent> > hookup(
      channel.get()->AddObserver(handler.get()));
  ScopedSessionContextSyncerEventChannel s_channel(context(), channel.get());

  dir->set_store_birthday(mock_server()->store_birthday());
  mock_server()->SetClearUserDataResponseStatus(sync_pb::SUCCESS);
  on_should_stop_syncing_permanently_called_ = false;

  command_.Execute(session());

  // Expect that the client sent a clear request, fired off the success event
  // in response, and disabled sync
  const sync_pb::ClientToServerMessage& r = mock_server()->last_request();
  EXPECT_TRUE(r.has_clear_user_data());

  EXPECT_TRUE(handler.get()->ReceievedClearSuccessEvent());
  EXPECT_TRUE(on_should_stop_syncing_permanently_called_);

  EXPECT_FALSE(handler->ReceievedClearFailedEvent());
}

}  // namespace browser_sync

