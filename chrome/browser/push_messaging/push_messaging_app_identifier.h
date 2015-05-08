// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_
#define CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// The prefix used for all push messaging application ids.
extern const char kPushMessagingAppIdentifierPrefix[];

// Type used to identify a web app from a Push API perspective.
// These can be persisted to disk, in a 1:1 mapping between app_id and
// pair<origin, service_worker_registration_id>.
class PushMessagingAppIdentifier {
 public:
  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Generates a new app identifier with random app_id.
  static PushMessagingAppIdentifier Generate(
      const GURL& origin,
      int64 service_worker_registration_id);

  // Looks up an app identifier by app_id. Will be invalid if not found.
  static PushMessagingAppIdentifier Get(Profile* profile,
                                        const std::string& app_id);

  // Looks up an app identifier by origin & service worker registration id.
  // Will be invalid if not found.
  static PushMessagingAppIdentifier Get(Profile* profile,
                                        const GURL& origin,
                                        int64 service_worker_registration_id);

  // Returns all the PushMessagingAppIdentifiers currently registered for the
  // given |profile|.
  static std::vector<PushMessagingAppIdentifier> GetAll(Profile* profile);

  ~PushMessagingAppIdentifier();

  // Persist this app identifier to disk.
  void PersistToDisk(Profile* profile) const;

  // Delete this app identifier from disk.
  void DeleteFromDisk(Profile* profile) const; // TODO: Does const make sense?

  bool IsValid() const;

  const std::string& app_id() const { return app_id_; }
  const GURL& origin() const { return origin_; }
  int64 service_worker_registration_id() const {
    return service_worker_registration_id_;
  }

 private:
  friend class PushMessagingAppIdentifierTest;

  // Constructs an invalid app identifier.
  PushMessagingAppIdentifier();
  // Constructs a valid app identifier.
  PushMessagingAppIdentifier(const std::string& app_id,
                             const GURL& origin,
                             int64 service_worker_registration_id);

  std::string app_id_;
  GURL origin_;
  int64 service_worker_registration_id_;
};

#endif  // CHROME_BROWSER_PUSH_MESSAGING_PUSH_MESSAGING_APP_IDENTIFIER_H_
