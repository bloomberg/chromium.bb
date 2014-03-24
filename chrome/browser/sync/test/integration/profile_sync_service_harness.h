// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"

class Profile;
class ProfileSyncService;

// An instance of this class is basically our notion of a "sync client" for
// automation purposes. It harnesses the ProfileSyncService member of the
// profile passed to it on construction and automates certain things like setup
// and authentication. It provides ways to "wait" adequate periods of time for
// several clients to get to the same state.
class ProfileSyncServiceHarness {
 public:
  static ProfileSyncServiceHarness* Create(
      Profile* profile,
      const std::string& username,
      const std::string& password);
  virtual ~ProfileSyncServiceHarness();

  // Sets the GAIA credentials with which to sign in to sync.
  void SetCredentials(const std::string& username, const std::string& password);

  // Creates a ProfileSyncService for the profile passed at construction and
  // enables sync for all available datatypes. Returns true only after sync has
  // been fully initialized and authenticated, and we are ready to process
  // changes.
  bool SetupSync();

  // Same as the above method, but enables sync only for the datatypes contained
  // in |synced_datatypes|.
  bool SetupSync(syncer::ModelTypeSet synced_datatypes);

  // Calling this acts as a barrier and blocks the caller until |this| and
  // |partner| have both completed a sync cycle.  When calling this method,
  // the |partner| should be the passive responder who responds to the actions
  // of |this|.  This method relies upon the synchronization of callbacks
  // from the message queue. Returns true if two sync cycles have completed.
  // Note: Use this method when exactly one client makes local change(s), and
  // exactly one client is waiting to receive those changes.
  bool AwaitMutualSyncCycleCompletion(ProfileSyncServiceHarness* partner);

  // Blocks the caller until |this| completes its ongoing sync cycle and every
  // other client in |partners| have achieved identical download progresses.
  // Note: Use this method when exactly one client makes local change(s),
  // and more than one client is waiting to receive those changes.
  bool AwaitGroupSyncCycleCompletion(
      std::vector<ProfileSyncServiceHarness*>& partners);

  // Blocks the caller until every client in |clients| completes its ongoing
  // sync cycle and all the clients' progress markers match.  Note: Use this
  // method when more than one client makes local change(s), and more than one
  // client is waiting to receive those changes.
  static bool AwaitQuiescence(
      std::vector<ProfileSyncServiceHarness*>& clients);

  // Returns the ProfileSyncService member of the sync client.
  ProfileSyncService* service() const { return service_; }

  // Returns the debug name for this profile. Used for logging.
  const std::string& profile_debug_name() const { return profile_debug_name_; }

  // Enables sync for a particular sync datatype. Returns true on success.
  bool EnableSyncForDatatype(syncer::ModelType datatype);

  // Disables sync for a particular sync datatype. Returns true on success.
  bool DisableSyncForDatatype(syncer::ModelType datatype);

  // Enables sync for all sync datatypes. Returns true on success.
  bool EnableSyncForAllDatatypes();

  // Disables sync for all sync datatypes. Returns true on success.
  bool DisableSyncForAllDatatypes();

  // Returns a snapshot of the current sync session.
  syncer::sessions::SyncSessionSnapshot GetLastSessionSnapshot() const;

  // Check if |type| is being synced.
  bool IsTypePreferred(syncer::ModelType type);

  // Returns a string that can be used as the value of an oauth2 refresh token.
  // This function guarantees that a different string is returned each time
  // it is called.
  std::string GenerateFakeOAuth2RefreshTokenString();

  // Returns a string with relevant info about client's sync state (if
  // available), annotated with |message|. Useful for logging.
  std::string GetClientInfoString(const std::string& message) const;

 private:
  ProfileSyncServiceHarness(
      Profile* profile,
      const std::string& username,
      const std::string& password);

  // Signals that sync setup is complete, and that PSS may begin syncing.
  void FinishSyncSetup();

  // Gets detailed status from |service_| in pretty-printable form.
  std::string GetServiceStatus();

  // Returns true if sync is disabled for this client.
  bool IsSyncDisabled() const;

  // Sync profile associated with this sync client.
  Profile* profile_;

  // ProfileSyncService object associated with |profile_|.
  ProfileSyncService* service_;

  // Credentials used for GAIA authentication.
  std::string username_;
  std::string password_;

  // Number used by GenerateFakeOAuth2RefreshTokenString() to make sure that
  // all refresh tokens used in the tests are different.
  int oauth2_refesh_token_number_;

  // Used for logging.
  const std::string profile_debug_name_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceHarness);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_PROFILE_SYNC_SERVICE_HARNESS_H_
