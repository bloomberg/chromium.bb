// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
#define CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/android/usage_stats/website_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace usage_stats {

// Stores website events, suspensions and token to fully-qualified domain name
// (FQDN) mappings in LevelDB.
class UsageStatsDatabase {
 public:
  enum class Error { kNoError, kUnknownError };

  using EventsCallback =
      base::OnceCallback<void(Error, std::vector<WebsiteEvent>)>;

  using SuspensionsCallback =
      base::OnceCallback<void(Error, std::vector<std::string>)>;

  using TokenMap = std::map<std::string, std::string>;
  using TokenMappingsCallback = base::OnceCallback<void(Error, TokenMap)>;

  using StatusCallback = base::OnceCallback<void(Error)>;

  using UsageStatMap = std::map<std::string, UsageStat>;

  // Initializes the database with user |profile|.
  explicit UsageStatsDatabase(Profile* profile);

  // Initializes the database with a |ProtoDatabase|. Useful for testing.
  explicit UsageStatsDatabase(
      std::unique_ptr<leveldb_proto::ProtoDatabase<UsageStat>> proto_db);

  ~UsageStatsDatabase();

  void GetAllEvents(EventsCallback callback);

  // Get all events in range between |start| (inclusive) and |end| (exclusive),
  // where |start| and |end| are represented by milliseconds since Unix epoch.
  void QueryEventsInRange(int64_t start, int64_t end, EventsCallback callback);

  void AddEvents(std::vector<WebsiteEvent> events, StatusCallback callback);

  void DeleteAllEvents(StatusCallback callback);

  // Delete all events in range between |start| (inclusive) and |end|
  // (exclusive), where |start| and |end| are represented by milliseconds since
  // Unix epoch.
  void DeleteEventsInRange(int64_t start, int64_t end, StatusCallback callback);

  void DeleteEventsWithMatchingDomains(base::flat_set<std::string> domains,
                                       StatusCallback callback);

  void GetAllSuspensions(SuspensionsCallback callback);

  // Persists all the suspensions in |domains| and deletes any suspensions *not*
  // in |domains|.
  void SetSuspensions(base::flat_set<std::string> domains,
                      StatusCallback callback);

  void GetAllTokenMappings(TokenMappingsCallback callback);

  // Persists all the mappings in |mappings| and deletes any mappings *not* in
  // |mappings|. The map's key is the token, and its value is the FQDN.
  void SetTokenMappings(TokenMap mappings, StatusCallback callback);

 private:
  void OnUpdateEntries(StatusCallback callback, bool isSuccess);

  void OnLoadEntriesForGetAllEvents(
      EventsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<UsageStat>> stats);

  void OnLoadEntriesForQueryEventsInRange(
      EventsCallback callback,
      bool isSuccess,
      std::unique_ptr<UsageStatMap> stat_map);

  void OnLoadEntriesForDeleteEventsInRange(
      StatusCallback callback,
      bool isSuccess,
      std::unique_ptr<UsageStatMap> stat_map);

  void OnLoadEntriesForGetAllSuspensions(
      SuspensionsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<UsageStat>> stats);

  void OnLoadEntriesForGetAllTokenMappings(
      TokenMappingsCallback callback,
      bool isSuccess,
      std::unique_ptr<std::vector<UsageStat>> stats);

  std::unique_ptr<leveldb_proto::ProtoDatabase<UsageStat>> proto_db_;

  base::WeakPtrFactory<UsageStatsDatabase> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageStatsDatabase);
};

}  // namespace usage_stats

#endif  // CHROME_BROWSER_ANDROID_USAGE_STATS_USAGE_STATS_DATABASE_H_
