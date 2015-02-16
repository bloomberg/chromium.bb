// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_

#include <string>

#include "base/basictypes.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace gcm {

// The prefix used for all push messaging application ids.
extern const char kPushMessagingApplicationIdPrefix[];

// Type used to identify a web app from a Push API perspective.
// These can be persisted to disk, in a 1:1 mapping between app_id_guid and
// pair<origin, service_worker_registration_id>.
class PushMessagingApplicationId {
 public:
  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Generates a new application id with random app_id_guid.
  static PushMessagingApplicationId Generate(
      const GURL& origin,
      int64 service_worker_registration_id);

  // Looks up an application id by app_id_guid. Will be invalid if not found.
  static PushMessagingApplicationId Get(Profile* profile,
                                        const std::string& app_id_guid);

  // Looks up an application id by origin & service worker registration id.
  // Will be invalid if not found.
  static PushMessagingApplicationId Get(Profile* profile,
                                        const GURL& origin,
                                        int64 service_worker_registration_id);

  ~PushMessagingApplicationId();

  // Persist this application id to disk.
  void PersistToDisk(Profile* profile) const;

  // Delete this application id from disk.
  void DeleteFromDisk(Profile* profile) const; // TODO: Does const make sense?

  bool IsValid() const;

  const std::string app_id_guid;
  const GURL origin;
  const int64 service_worker_registration_id;

 private:
  friend class PushMessagingApplicationIdTest;

  // Constructs an invalid app id.
  PushMessagingApplicationId();
  // Constructs a valid app id.
  PushMessagingApplicationId(const std::string& app_id_guid,
                             const GURL& origin,
                             int64 service_worker_registration_id);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_APPLICATION_ID_H_
