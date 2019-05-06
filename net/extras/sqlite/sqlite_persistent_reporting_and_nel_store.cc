// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/extras/sqlite/sqlite_persistent_reporting_and_nel_store.h"

#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "net/extras/sqlite/sqlite_persistent_store_backend_base.h"
#include "net/reporting/reporting_endpoint.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace net {

namespace {
// Version 1: Adds tables for NEL policies, Reporting endpoints, and Reporting
//            endpoint groups.

const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;
}  // namespace

class SQLitePersistentReportingAndNELStore::Backend
    : public SQLitePersistentStoreBackendBase {
 public:
  Backend(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
      : SQLitePersistentStoreBackendBase(
            path,
            /* histogram_tag = */ "ReportingAndNEL",
            kCurrentVersionNumber,
            kCompatibleVersionNumber,
            background_task_runner,
            client_task_runner),
        num_pending_(0) {}

  void LoadNELPolicies(NELPoliciesLoadedCallback loaded_callback);
  void AddNELPolicy(const NetworkErrorLoggingService::NELPolicy& policy);
  void UpdateNELPolicyAccessTime(
      const NetworkErrorLoggingService::NELPolicy& policy);
  void DeleteNELPolicy(const NetworkErrorLoggingService::NELPolicy& policy);

  void LoadReportingClients(ReportingClientsLoadedCallback loaded_callback);
  void AddReportingEndpoint(const ReportingEndpoint& endpoint);
  void AddReportingEndpointGroup(const CachedReportingEndpointGroup& group);
  void UpdateReportingEndpointGroupAccessTime(
      const CachedReportingEndpointGroup& group);
  void UpdateReportingEndpointDetails(const ReportingEndpoint& endpoint);
  void UpdateReportingEndpointGroupDetails(
      const CachedReportingEndpointGroup& group);
  void DeleteReportingEndpoint(const ReportingEndpoint& endpoint);
  void DeleteReportingEndpointGroup(const CachedReportingEndpointGroup& group);

  // Gets the number of queued operations.
  size_t GetQueueLengthForTesting() const;

 private:
  ~Backend() override {
    DCHECK(nel_policy_pending_ops_.empty());
    DCHECK_EQ(0u, num_pending_);
  }

  // Represents a mutating operation to the database, specified by a type (add,
  // update access time, update data, or delete) and data representing the entry
  // in the database to be added/updated/deleted.
  template <typename DataType>
  class PendingOperation;

  // List of pending operations for a particular entry in the database.
  template <typename DataType>
  using PendingOperationsVector =
      std::vector<std::unique_ptr<PendingOperation<DataType>>>;

  // A copy of the information relevant to a NEL policy.
  struct NELPolicyInfo;
  // A copy of the information relevant to a Reporting endpoint.
  struct ReportingEndpointInfo;
  // A copy of the information relevant to a Reporting endpoint group.
  struct ReportingEndpointGroupInfo;
  // TODO(chlily): add ReportingReportInfo.

  // Uniquely identifies an endpoint in the store.
  using ReportingEndpointKey = std::pair<ReportingEndpointGroupKey, GURL>;

  // Map of pending operations for each entry in the database.
  // Key types are: - url::Origin for NEL policies,
  //                - ReportingEndpointKey for Reporting endpoints,
  //                - ReportingEndpointGroupKey for Reporting endpoint groups
  //                  (defined in //net/reporting/reporting_endpoint.h).
  template <typename KeyType, typename DataType>
  using QueueType = std::map<KeyType, PendingOperationsVector<DataType>>;

  // SQLitePersistentStoreBackendBase implementation
  bool CreateDatabaseSchema() override;
  base::Optional<int> DoMigrateDatabaseSchema() override;
  void DoCommit() override;

  // Commit a pending operation pertaining to a NEL policy.
  void CommitNELPolicyOperation(PendingOperation<NELPolicyInfo>* op);
  // Commit a pending operation pertaining to a Reporting endpoint.
  void CommitReportingEndpointOperation(
      PendingOperation<ReportingEndpointInfo>* op);
  // Commit a pending operation pertaining to a Reporting endpoint group.
  void CommitReportingEndpointGroupOperation(
      PendingOperation<ReportingEndpointGroupInfo>* op);

  // Add a pending operation to the appropriate queue.
  template <typename KeyType, typename DataType>
  void BatchOperation(KeyType key,
                      std::unique_ptr<PendingOperation<DataType>> po,
                      QueueType<KeyType, DataType>* queue);

  // If there are existing pending operations for a given key, potentially
  // remove some of the existing operations before adding |new_op|.
  // In particular, if |new_op| is a deletion, then all the previous pending
  // operations are made irrelevant and can be deleted. If |new_op| is an
  // update-access-time, and the last operation in |ops_for_key| is also an
  // update-access-time, then it can be discarded because |new_op| is about to
  // overwrite the access time with a new value anyway. Similarly for
  // update-details.
  template <typename DataType>
  void MaybeCoalesceOperations(PendingOperationsVector<DataType>* ops_for_key,
                               PendingOperation<DataType>* new_op)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // After adding a pending operation to one of the pending operations queues,
  // this method posts a task to commit all pending operations if we reached the
  // batch size, or starts a timer to commit after a time interval if we just
  // started a new batch. |num_pending| is the total number of pending
  // operations after the one we just added.
  void OnOperationBatched(size_t num_pending);

  // Loads NEL policies into a vector in the background, then posts a
  // task to the client task runner to call |loaded_callback| with the loaded
  // NEL policies.
  void LoadNELPoliciesAndNotifyInBackground(
      NELPoliciesLoadedCallback loaded_callback);

  // Calls |loaded_callback| with the loaded NEL policies (which may be empty if
  // loading was unsuccessful). If loading was successful, also report metrics.
  void CompleteLoadNELPoliciesAndNotifyInForeground(
      NELPoliciesLoadedCallback loaded_callback,
      std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies,
      bool load_success);

  // Loads Reporting endpoints and endpoint groups into two vectors in the
  // background, then posts a task to the client task runner to call
  // |loaded_callback| with the loaded endpoints and endpoint groups.
  void LoadReportingClientsAndNotifyInBackground(
      ReportingClientsLoadedCallback loaded_callback);

  // Calls |loaded_callback| with the loaded endpoints and endpoint groups
  // (which may be empty if loading was unsuccessful). If loading was
  // successful, also report metrics.
  void CompleteLoadReportingClientsAndNotifyInForeground(
      ReportingClientsLoadedCallback loaded_callback,
      std::vector<ReportingEndpoint> loaded_endpoints,
      std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups,
      bool load_success);

  // Total number of pending operations (may not match the sum of the number of
  // elements in the pending operations queues, due to operation coalescing).
  size_t num_pending_ GUARDED_BY(lock_);

  // Queue of pending operations pertaining to NEL policies, keyed on origin.
  QueueType<url::Origin, NELPolicyInfo> nel_policy_pending_ops_
      GUARDED_BY(lock_);
  // Queue of pending operations pertaining to Reporting endpoints, keyed on
  // origin, group name, and url.
  QueueType<ReportingEndpointKey, ReportingEndpointInfo>
      reporting_endpoint_pending_ops_ GUARDED_BY(lock_);
  // Queue of pending operations pertaining to Reporting endpoint groups, keyed
  // on origin and group name.
  QueueType<ReportingEndpointGroupKey, ReportingEndpointGroupInfo>
      reporting_endpoint_group_pending_ops_ GUARDED_BY(lock_);

  // TODO(chlily): add reporting_report_pending_ops_ for Reporting reports.

  // Protects |num_pending_|, and all the pending operations queues.
  mutable base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

namespace {

bool CreateV1NELPoliciesSchema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("nel_policies"));

  std::string stmt =
      "CREATE TABLE nel_policies ("
      "  origin_scheme TEXT NOT NULL,"
      "  origin_host TEXT NOT NULL,"
      "  origin_port INTEGER NOT NULL,"
      "  received_ip_address TEXT NOT NULL,"
      "  report_to TEXT NOT NULL,"
      "  expires_us_since_epoch INTEGER NOT NULL,"
      "  success_fraction REAL NOT NULL,"
      "  failure_fraction REAL NOT NULL,"
      "  is_include_subdomains INTEGER NOT NULL,"
      "  last_access_us_since_epoch INTEGER NOT NULL,"
      // Each origin specifies at most one NEL policy.
      "  UNIQUE (origin_scheme, origin_host, origin_port)"
      ")";

  return db->Execute(stmt.c_str());
}

bool CreateV1ReportingEndpointsSchema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("reporting_endpoints"));

  std::string stmt =
      "CREATE TABLE reporting_endpoints ("
      "  origin_scheme TEXT NOT NULL,"
      "  origin_host TEXT NOT NULL,"
      "  origin_port INTEGER NOT NULL,"
      "  group_name TEXT NOT NULL,"
      "  url TEXT NOT NULL,"
      "  priority INTEGER NOT NULL,"
      "  weight INTEGER NOT NULL,"
      // Each (origin, group, url) tuple specifies at most one endpoint.
      "  UNIQUE (origin_scheme, origin_host, origin_port, group_name, url)"
      ")";

  return db->Execute(stmt.c_str());
}

bool CreateV1ReportingEndpointGroupsSchema(sql::Database* db) {
  DCHECK(!db->DoesTableExist("reporting_endpoint_groups"));

  std::string stmt =
      "CREATE TABLE reporting_endpoint_groups ("
      "  origin_scheme TEXT NOT NULL,"
      "  origin_host TEXT NOT NULL,"
      "  origin_port INTEGER NOT NULL,"
      "  group_name TEXT NOT NULL,"
      "  is_include_subdomains INTEGER NOT NULL,"
      "  expires_us_since_epoch INTEGER NOT NULL,"
      "  last_access_us_since_epoch INTEGER NOT NULL,"
      // Each (origin, group) tuple specifies at most one endpoint group.
      "  UNIQUE (origin_scheme, origin_host, origin_port, group_name)"
      ")";

  return db->Execute(stmt.c_str());
}

}  // namespace

template <typename DataType>
class SQLitePersistentReportingAndNELStore::Backend::PendingOperation {
 public:
  enum class Type { ADD, UPDATE_ACCESS_TIME, UPDATE_DETAILS, DELETE };

  PendingOperation(Type type, DataType data)
      : type_(type), data_(std::move(data)) {}

  Type type() const { return type_; }
  const DataType& data() const { return data_; }

 private:
  const Type type_;
  const DataType data_;
};

// Makes a copy of the relevant information about a NELPolicy, stored in a
// form suitable for adding to the database.
struct SQLitePersistentReportingAndNELStore::Backend::NELPolicyInfo {
  NELPolicyInfo(const NetworkErrorLoggingService::NELPolicy& nel_policy)
      : origin_scheme(nel_policy.origin.scheme()),
        origin_host(nel_policy.origin.host()),
        origin_port(nel_policy.origin.port()),
        received_ip_address(nel_policy.received_ip_address.ToString()),
        report_to(nel_policy.report_to),
        expires_us_since_epoch(
            nel_policy.expires.ToDeltaSinceWindowsEpoch().InMicroseconds()),
        success_fraction(nel_policy.success_fraction),
        failure_fraction(nel_policy.failure_fraction),
        is_include_subdomains(nel_policy.include_subdomains),
        last_access_us_since_epoch(
            nel_policy.last_used.ToDeltaSinceWindowsEpoch().InMicroseconds()) {}

  // Origin the policy was received from.
  std::string origin_scheme;
  std::string origin_host;
  int origin_port = 0;
  // IP address of the server that the policy was received from.
  std::string received_ip_address;
  // The Reporting group which the policy specifies.
  std::string report_to;
  // When the policy expires, in microseconds since the Windows epoch.
  int64_t expires_us_since_epoch = 0;
  // Sampling fractions.
  double success_fraction = 0.0;
  double failure_fraction = 1.0;
  // Whether the policy applies to subdomains of the origin.
  bool is_include_subdomains = false;
  // Last time the policy was updated or used, in microseconds since the
  // Windows epoch.
  int64_t last_access_us_since_epoch = 0;
};

// Makes a copy of the relevant information about a ReportingEndpoint, stored in
// a form suitable for adding to the database.
struct SQLitePersistentReportingAndNELStore::Backend::ReportingEndpointInfo {
  ReportingEndpointInfo(const ReportingEndpoint& endpoint)
      : origin_scheme(endpoint.group_key.origin.scheme()),
        origin_host(endpoint.group_key.origin.host()),
        origin_port(endpoint.group_key.origin.port()),
        group_name(endpoint.group_key.group_name),
        url(endpoint.info.url.spec()),
        priority(endpoint.info.priority),
        weight(endpoint.info.weight) {}

  // Origin the endpoint was received from.
  std::string origin_scheme;
  std::string origin_host;
  int origin_port = 0;
  // Name of the group the endpoint belongs to.
  std::string group_name;
  // URL of the endpoint.
  std::string url;
  // Priority of the endpoint.
  int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority;
  // Weight of the endpoint.
  int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight;
};

struct SQLitePersistentReportingAndNELStore::Backend::
    ReportingEndpointGroupInfo {
  ReportingEndpointGroupInfo(const CachedReportingEndpointGroup& group)
      : origin_scheme(group.group_key.origin.scheme()),
        origin_host(group.group_key.origin.host()),
        origin_port(group.group_key.origin.port()),
        group_name(group.group_key.group_name),
        is_include_subdomains(group.include_subdomains ==
                              OriginSubdomains::INCLUDE),
        expires_us_since_epoch(
            group.expires.ToDeltaSinceWindowsEpoch().InMicroseconds()),
        last_access_us_since_epoch(
            group.last_used.ToDeltaSinceWindowsEpoch().InMicroseconds()) {}

  // Origin the endpoint group was received from.
  std::string origin_scheme;
  std::string origin_host;
  int origin_port = 0;
  // Name of the group.
  std::string group_name;
  // Whether the group applies to subdomains of the origin.
  bool is_include_subdomains = false;
  // When the group expires, in microseconds since the Windows epoch.
  int64_t expires_us_since_epoch = 0;
  // Last time the group was updated or used, in microseconds since the Windows
  // epoch.
  int64_t last_access_us_since_epoch = 0;
};

void SQLitePersistentReportingAndNELStore::Backend::LoadNELPolicies(
    NELPoliciesLoadedCallback loaded_callback) {
  PostBackgroundTask(
      FROM_HERE, base::BindOnce(&Backend::LoadNELPoliciesAndNotifyInBackground,
                                this, std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::Backend::AddNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::ADD, NELPolicyInfo(policy));
  BatchOperation(policy.origin, std::move(po), &nel_policy_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::UpdateNELPolicyAccessTime(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::UPDATE_ACCESS_TIME,
      NELPolicyInfo(policy));
  BatchOperation(policy.origin, std::move(po), &nel_policy_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::DeleteNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  auto po = std::make_unique<PendingOperation<NELPolicyInfo>>(
      PendingOperation<NELPolicyInfo>::Type::DELETE, NELPolicyInfo(policy));
  BatchOperation(policy.origin, std::move(po), &nel_policy_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::LoadReportingClients(
    ReportingClientsLoadedCallback loaded_callback) {
  PostBackgroundTask(
      FROM_HERE,
      base::BindOnce(&Backend::LoadReportingClientsAndNotifyInBackground, this,
                     std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::Backend::AddReportingEndpoint(
    const ReportingEndpoint& endpoint) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointInfo>>(
      PendingOperation<ReportingEndpointInfo>::Type::ADD,
      ReportingEndpointInfo(endpoint));
  ReportingEndpointKey key =
      std::make_pair(endpoint.group_key, endpoint.info.url);
  BatchOperation(std::move(key), std::move(po),
                 &reporting_endpoint_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::AddReportingEndpointGroup(
    const CachedReportingEndpointGroup& group) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointGroupInfo>>(
      PendingOperation<ReportingEndpointGroupInfo>::Type::ADD,
      ReportingEndpointGroupInfo(group));
  BatchOperation(group.group_key, std::move(po),
                 &reporting_endpoint_group_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::
    UpdateReportingEndpointGroupAccessTime(
        const CachedReportingEndpointGroup& group) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointGroupInfo>>(
      PendingOperation<ReportingEndpointGroupInfo>::Type::UPDATE_ACCESS_TIME,
      ReportingEndpointGroupInfo(group));
  BatchOperation(group.group_key, std::move(po),
                 &reporting_endpoint_group_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::
    UpdateReportingEndpointDetails(const ReportingEndpoint& endpoint) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointInfo>>(
      PendingOperation<ReportingEndpointInfo>::Type::UPDATE_DETAILS,
      ReportingEndpointInfo(endpoint));
  ReportingEndpointKey key =
      std::make_pair(endpoint.group_key, endpoint.info.url);
  BatchOperation(std::move(key), std::move(po),
                 &reporting_endpoint_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::
    UpdateReportingEndpointGroupDetails(
        const CachedReportingEndpointGroup& group) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointGroupInfo>>(
      PendingOperation<ReportingEndpointGroupInfo>::Type::UPDATE_DETAILS,
      ReportingEndpointGroupInfo(group));
  BatchOperation(group.group_key, std::move(po),
                 &reporting_endpoint_group_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::DeleteReportingEndpoint(
    const ReportingEndpoint& endpoint) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointInfo>>(
      PendingOperation<ReportingEndpointInfo>::Type::DELETE,
      ReportingEndpointInfo(endpoint));
  ReportingEndpointKey key =
      std::make_pair(endpoint.group_key, endpoint.info.url);
  BatchOperation(std::move(key), std::move(po),
                 &reporting_endpoint_pending_ops_);
}

void SQLitePersistentReportingAndNELStore::Backend::
    DeleteReportingEndpointGroup(const CachedReportingEndpointGroup& group) {
  auto po = std::make_unique<PendingOperation<ReportingEndpointGroupInfo>>(
      PendingOperation<ReportingEndpointGroupInfo>::Type::DELETE,
      ReportingEndpointGroupInfo(group));
  BatchOperation(group.group_key, std::move(po),
                 &reporting_endpoint_group_pending_ops_);
}

size_t SQLitePersistentReportingAndNELStore::Backend::GetQueueLengthForTesting()
    const {
  size_t count = 0;
  {
    base::AutoLock locked(lock_);
    for (auto& key_and_pending_ops : nel_policy_pending_ops_) {
      count += key_and_pending_ops.second.size();
    }
    for (auto& key_and_pending_ops : reporting_endpoint_pending_ops_) {
      count += key_and_pending_ops.second.size();
    }
    for (auto& key_and_pending_ops : reporting_endpoint_group_pending_ops_) {
      count += key_and_pending_ops.second.size();
    }
  }
  return count;
}

bool SQLitePersistentReportingAndNELStore::Backend::CreateDatabaseSchema() {
  if (!db()->DoesTableExist("nel_policies") &&
      !CreateV1NELPoliciesSchema(db())) {
    return false;
  }

  if (!db()->DoesTableExist("reporting_endpoints") &&
      !CreateV1ReportingEndpointsSchema(db())) {
    return false;
  }

  if (!db()->DoesTableExist("reporting_endpoint_groups") &&
      !CreateV1ReportingEndpointGroupsSchema(db())) {
    return false;
  }

  // TODO(chlily): Initialize tables for Reporting reports.

  return true;
}

base::Optional<int>
SQLitePersistentReportingAndNELStore::Backend::DoMigrateDatabaseSchema() {
  int cur_version = meta_table()->GetVersionNumber();
  if (cur_version != 1)
    return base::nullopt;

  // Future database upgrade statements go here.

  return base::make_optional(cur_version);
}

void SQLitePersistentReportingAndNELStore::Backend::DoCommit() {
  QueueType<url::Origin, NELPolicyInfo> nel_policy_ops;
  QueueType<ReportingEndpointKey, ReportingEndpointInfo> reporting_endpoint_ops;
  QueueType<ReportingEndpointGroupKey, ReportingEndpointGroupInfo>
      reporting_endpoint_group_ops;
  size_t op_count = 0;
  {
    base::AutoLock locked(lock_);
    nel_policy_pending_ops_.swap(nel_policy_ops);
    reporting_endpoint_pending_ops_.swap(reporting_endpoint_ops);
    reporting_endpoint_group_pending_ops_.swap(reporting_endpoint_group_ops);
    // TODO(chlily): swap out pending operations queue for Reporting reports.
    op_count = num_pending_;
    num_pending_ = 0;
  }
  if (!db() || op_count == 0)
    return;

  sql::Transaction transaction(db());
  if (!transaction.Begin())
    return;

  // Commit all the NEL policy operations.
  for (const auto& origin_and_nel_policy_ops : nel_policy_ops) {
    const PendingOperationsVector<NELPolicyInfo>& ops_for_origin =
        origin_and_nel_policy_ops.second;
    for (const std::unique_ptr<PendingOperation<NELPolicyInfo>>& nel_policy_op :
         ops_for_origin) {
      CommitNELPolicyOperation(nel_policy_op.get());
    }
  }

  // Commit all the Reporting endpoint operations.
  for (const auto& key_and_reporting_endpoint_ops : reporting_endpoint_ops) {
    const PendingOperationsVector<ReportingEndpointInfo>& ops_for_key =
        key_and_reporting_endpoint_ops.second;
    for (const std::unique_ptr<PendingOperation<ReportingEndpointInfo>>&
             reporting_endpoint_op : ops_for_key) {
      CommitReportingEndpointOperation(reporting_endpoint_op.get());
    }
  }

  // Commit all the Reporting endpoint group operations.
  for (const auto& key_and_reporting_endpoint_group_ops :
       reporting_endpoint_group_ops) {
    const PendingOperationsVector<ReportingEndpointGroupInfo>& ops_for_key =
        key_and_reporting_endpoint_group_ops.second;
    for (const std::unique_ptr<PendingOperation<ReportingEndpointGroupInfo>>&
             reporting_endpoint_group_op : ops_for_key) {
      CommitReportingEndpointGroupOperation(reporting_endpoint_group_op.get());
    }
  }

  // TODO(chlily): Commit operations pertaining to Reporting reports.

  transaction.Commit();
}

void SQLitePersistentReportingAndNELStore::Backend::CommitNELPolicyOperation(
    PendingOperation<NELPolicyInfo>* op) {
  DCHECK_EQ(1, db()->transaction_nesting());

  sql::Statement add_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO nel_policies (origin_scheme, origin_host, origin_port, "
      "received_ip_address, report_to, expires_us_since_epoch, "
      "success_fraction, failure_fraction, is_include_subdomains, "
      "last_access_us_since_epoch) VALUES (?,?,?,?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement update_access_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE nel_policies SET last_access_us_since_epoch=? WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=?"));
  if (!update_access_smt.is_valid())
    return;

  sql::Statement del_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM nel_policies WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=?"));
  if (!del_smt.is_valid())
    return;

  const NELPolicyInfo& nel_policy_info = op->data();

  switch (op->type()) {
    case PendingOperation<NELPolicyInfo>::Type::ADD:
      add_smt.Reset(true);
      add_smt.BindString(0, nel_policy_info.origin_scheme);
      add_smt.BindString(1, nel_policy_info.origin_host);
      add_smt.BindInt(2, nel_policy_info.origin_port);
      add_smt.BindString(3, nel_policy_info.received_ip_address);
      add_smt.BindString(4, nel_policy_info.report_to);
      add_smt.BindInt64(5, nel_policy_info.expires_us_since_epoch);
      add_smt.BindDouble(6, nel_policy_info.success_fraction);
      add_smt.BindDouble(7, nel_policy_info.failure_fraction);
      add_smt.BindBool(8, nel_policy_info.is_include_subdomains);
      add_smt.BindInt64(9, nel_policy_info.last_access_us_since_epoch);
      if (!add_smt.Run())
        DLOG(WARNING) << "Could not add a NEL policy to the DB.";
      break;

    case PendingOperation<NELPolicyInfo>::Type::UPDATE_ACCESS_TIME:
      update_access_smt.Reset(true);
      update_access_smt.BindInt64(0,
                                  nel_policy_info.last_access_us_since_epoch);
      update_access_smt.BindString(1, nel_policy_info.origin_scheme);
      update_access_smt.BindString(2, nel_policy_info.origin_host);
      update_access_smt.BindInt(3, nel_policy_info.origin_port);
      if (!update_access_smt.Run()) {
        DLOG(WARNING)
            << "Could not update NEL policy last access time in the DB.";
      }
      break;

    case PendingOperation<NELPolicyInfo>::Type::DELETE:
      del_smt.Reset(true);
      del_smt.BindString(0, nel_policy_info.origin_scheme);
      del_smt.BindString(1, nel_policy_info.origin_host);
      del_smt.BindInt(2, nel_policy_info.origin_port);
      if (!del_smt.Run())
        DLOG(WARNING) << "Could not delete a NEL policy from the DB.";
      break;

    default:
      // There are no UPDATE_DETAILS operations for NEL policies.
      // TODO(chlily): Maybe add the ability to update details as opposed to
      // removing and re-adding every time; it might be slightly more efficient.
      NOTREACHED();
      break;
  }
}

void SQLitePersistentReportingAndNELStore::Backend::
    CommitReportingEndpointOperation(
        PendingOperation<ReportingEndpointInfo>* op) {
  DCHECK_EQ(1, db()->transaction_nesting());

  sql::Statement add_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO reporting_endpoints (origin_scheme, origin_host, "
      "origin_port, group_name, url, priority, weight) "
      "VALUES (?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement update_details_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE reporting_endpoints SET priority=?, weight=? WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=? "
      "AND group_name=? AND url=?"));
  if (!update_details_smt.is_valid())
    return;

  sql::Statement del_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM reporting_endpoints WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=? "
      "AND group_name=? AND url=?"));
  if (!del_smt.is_valid())
    return;

  const ReportingEndpointInfo& reporting_endpoint_info = op->data();

  switch (op->type()) {
    case PendingOperation<ReportingEndpointInfo>::Type::ADD:
      add_smt.Reset(true);
      add_smt.BindString(0, reporting_endpoint_info.origin_scheme);
      add_smt.BindString(1, reporting_endpoint_info.origin_host);
      add_smt.BindInt(2, reporting_endpoint_info.origin_port);
      add_smt.BindString(3, reporting_endpoint_info.group_name);
      add_smt.BindString(4, reporting_endpoint_info.url);
      add_smt.BindInt(5, reporting_endpoint_info.priority);
      add_smt.BindInt(6, reporting_endpoint_info.weight);
      if (!add_smt.Run())
        DLOG(WARNING) << "Could not add a Reporting endpoint to the DB.";
      break;

    case PendingOperation<ReportingEndpointInfo>::Type::UPDATE_DETAILS:
      update_details_smt.Reset(true);
      update_details_smt.BindInt(0, reporting_endpoint_info.priority);
      update_details_smt.BindInt(1, reporting_endpoint_info.weight);
      update_details_smt.BindString(2, reporting_endpoint_info.origin_scheme);
      update_details_smt.BindString(3, reporting_endpoint_info.origin_host);
      update_details_smt.BindInt(4, reporting_endpoint_info.origin_port);
      update_details_smt.BindString(5, reporting_endpoint_info.group_name);
      update_details_smt.BindString(6, reporting_endpoint_info.url);
      if (!update_details_smt.Run()) {
        DLOG(WARNING)
            << "Could not update Reporting endpoint details in the DB.";
      }
      break;

    case PendingOperation<ReportingEndpointInfo>::Type::DELETE:
      del_smt.Reset(true);
      del_smt.BindString(0, reporting_endpoint_info.origin_scheme);
      del_smt.BindString(1, reporting_endpoint_info.origin_host);
      del_smt.BindInt(2, reporting_endpoint_info.origin_port);
      del_smt.BindString(3, reporting_endpoint_info.group_name);
      del_smt.BindString(4, reporting_endpoint_info.url);
      if (!del_smt.Run())
        DLOG(WARNING) << "Could not delete a Reporting endpoint from the DB.";
      break;

    default:
      // There are no UPDATE_ACCESS_TIME operations for Reporting endpoints
      // because their access times are not tracked.
      NOTREACHED();
      break;
  }
}

void SQLitePersistentReportingAndNELStore::Backend::
    CommitReportingEndpointGroupOperation(
        PendingOperation<ReportingEndpointGroupInfo>* op) {
  DCHECK_EQ(1, db()->transaction_nesting());

  sql::Statement add_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO reporting_endpoint_groups (origin_scheme, origin_host, "
      "origin_port, group_name, is_include_subdomains, expires_us_since_epoch, "
      "last_access_us_since_epoch) VALUES (?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement update_access_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE reporting_endpoint_groups SET last_access_us_since_epoch=? WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=? AND group_name=?"));
  if (!update_access_smt.is_valid())
    return;

  sql::Statement update_details_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE reporting_endpoint_groups SET is_include_subdomains=?, "
      "expires_us_since_epoch=?, last_access_us_since_epoch=? WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=? AND group_name=?"));
  if (!update_details_smt.is_valid())
    return;

  sql::Statement del_smt(db()->GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM reporting_endpoint_groups WHERE "
      "origin_scheme=? AND origin_host=? AND origin_port=? AND group_name=?"));
  if (!del_smt.is_valid())
    return;

  const ReportingEndpointGroupInfo& reporting_endpoint_group_info = op->data();

  switch (op->type()) {
    case PendingOperation<ReportingEndpointGroupInfo>::Type::ADD:
      add_smt.Reset(true);
      add_smt.BindString(0, reporting_endpoint_group_info.origin_scheme);
      add_smt.BindString(1, reporting_endpoint_group_info.origin_host);
      add_smt.BindInt(2, reporting_endpoint_group_info.origin_port);
      add_smt.BindString(3, reporting_endpoint_group_info.group_name);
      add_smt.BindBool(4, reporting_endpoint_group_info.is_include_subdomains);
      add_smt.BindInt64(5,
                        reporting_endpoint_group_info.expires_us_since_epoch);
      add_smt.BindInt64(
          6, reporting_endpoint_group_info.last_access_us_since_epoch);
      if (!add_smt.Run())
        DLOG(WARNING) << "Could not add a Reporting endpoint group to the DB.";
      break;

    case PendingOperation<ReportingEndpointGroupInfo>::Type::UPDATE_ACCESS_TIME:
      update_access_smt.Reset(true);
      update_access_smt.BindInt64(
          0, reporting_endpoint_group_info.last_access_us_since_epoch);
      update_access_smt.BindString(1,
                                   reporting_endpoint_group_info.origin_scheme);
      update_access_smt.BindString(2,
                                   reporting_endpoint_group_info.origin_host);
      update_access_smt.BindInt(3, reporting_endpoint_group_info.origin_port);
      update_access_smt.BindString(4, reporting_endpoint_group_info.group_name);
      if (!update_access_smt.Run()) {
        DLOG(WARNING)
            << "Could not update Reporting endpoint group last access "
               "time in the DB.";
      }
      break;

    case PendingOperation<ReportingEndpointGroupInfo>::Type::UPDATE_DETAILS:
      update_details_smt.Reset(true);
      update_details_smt.BindBool(
          0, reporting_endpoint_group_info.is_include_subdomains);
      update_details_smt.BindInt64(
          1, reporting_endpoint_group_info.expires_us_since_epoch);
      update_details_smt.BindInt64(
          2, reporting_endpoint_group_info.last_access_us_since_epoch);
      update_details_smt.BindString(
          3, reporting_endpoint_group_info.origin_scheme);
      update_details_smt.BindString(4,
                                    reporting_endpoint_group_info.origin_host);
      update_details_smt.BindInt(5, reporting_endpoint_group_info.origin_port);
      update_details_smt.BindString(6,
                                    reporting_endpoint_group_info.group_name);
      if (!update_details_smt.Run()) {
        DLOG(WARNING)
            << "Could not update Reporting endpoint group details in the DB.";
      }
      break;

    case PendingOperation<ReportingEndpointGroupInfo>::Type::DELETE:
      del_smt.Reset(true);
      del_smt.BindString(0, reporting_endpoint_group_info.origin_scheme);
      del_smt.BindString(1, reporting_endpoint_group_info.origin_host);
      del_smt.BindInt(2, reporting_endpoint_group_info.origin_port);
      del_smt.BindString(3, reporting_endpoint_group_info.group_name);
      if (!del_smt.Run()) {
        DLOG(WARNING)
            << "Could not delete a Reporting endpoint group from the DB.";
      }
      break;
  }
}

template <typename KeyType, typename DataType>
void SQLitePersistentReportingAndNELStore::Backend::BatchOperation(
    KeyType key,
    std::unique_ptr<PendingOperation<DataType>> po,
    QueueType<KeyType, DataType>* queue) {
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());

  size_t num_pending;
  {
    base::AutoLock locked(lock_);

    std::pair<typename QueueType<KeyType, DataType>::iterator, bool>
        iter_and_result = queue->insert(std::make_pair(
            std::move(key), PendingOperationsVector<DataType>()));
    PendingOperationsVector<DataType>* ops_for_key =
        &iter_and_result.first->second;
    // If the insert failed, then we already have operations for this
    // key, so we try to coalesce the new operation with the existing ones.
    if (!iter_and_result.second)
      MaybeCoalesceOperations(ops_for_key, po.get());
    ops_for_key->push_back(std::move(po));
    // Note that num_pending_ counts number of calls to Batch*Operation(), not
    // the current length of the queue; this is intentional to guarantee
    // progress, as the length of the queue may decrease in some cases.
    num_pending = ++num_pending_;
  }

  OnOperationBatched(num_pending);
}

template <typename DataType>
void SQLitePersistentReportingAndNELStore::Backend::MaybeCoalesceOperations(
    PendingOperationsVector<DataType>* ops_for_key,
    PendingOperation<DataType>* new_op) {
  DCHECK(!ops_for_key->empty());

  switch (new_op->type()) {
    case PendingOperation<DataType>::Type::DELETE:
      // A delete makes all previous operations irrelevant.
      ops_for_key->clear();
      break;

    case PendingOperation<DataType>::Type::UPDATE_ACCESS_TIME:
      if (ops_for_key->back()->type() ==
          PendingOperation<DataType>::Type::UPDATE_ACCESS_TIME) {
        // Updating the access time twice in a row is equivalent to just the
        // latter update.
        ops_for_key->pop_back();
      }
      break;

    case PendingOperation<DataType>::Type::UPDATE_DETAILS:
      while (!ops_for_key->empty() &&
             // Updating the details twice in a row is equivalent to just the
             // latter update.
             (ops_for_key->back()->type() ==
                  PendingOperation<DataType>::Type::UPDATE_DETAILS ||
              // UPDATE_DETAILS also updates the access time, so either type of
              // update operation can be discarded.
              ops_for_key->back()->type() ==
                  PendingOperation<DataType>::Type::UPDATE_ACCESS_TIME)) {
        ops_for_key->pop_back();
      }
      break;

    case PendingOperation<DataType>::Type::ADD:
      // Nothing special is done for an add operation. If it is overwriting an
      // existing entry, it will be preceded by at most one delete.
      DCHECK_LE(ops_for_key->size(), 1u);
      break;
  }
}

void SQLitePersistentReportingAndNELStore::Backend::OnOperationBatched(
    size_t num_pending) {
  DCHECK(!background_task_runner()->RunsTasksInCurrentSequence());
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    if (!background_task_runner()->PostDelayedTask(
            FROM_HERE, base::BindOnce(&Backend::Commit, this),
            base::TimeDelta::FromMilliseconds(kCommitIntervalMs))) {
      NOTREACHED() << "background_task_runner_ is not running.";
    }
  } else if (num_pending >= kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    PostBackgroundTask(FROM_HERE, base::BindOnce(&Backend::Commit, this));
  }
}

// TODO(chlily): Discard expired policies when loading, discard and record
// problem if loaded policy is malformed.
void SQLitePersistentReportingAndNELStore::Backend::
    LoadNELPoliciesAndNotifyInBackground(
        NELPoliciesLoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies;
  if (!InitializeDatabase()) {
    PostClientTask(
        FROM_HERE,
        base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                       this, std::move(loaded_callback),
                       std::move(loaded_policies), false /* load_success */));
    return;
  }

  sql::Statement smt(db()->GetUniqueStatement(
      "SELECT origin_scheme, origin_host, origin_port, received_ip_address, "
      "report_to, expires_us_since_epoch, success_fraction, failure_fraction, "
      "is_include_subdomains, last_access_us_since_epoch FROM nel_policies"));
  if (!smt.is_valid()) {
    Reset();
    PostClientTask(
        FROM_HERE,
        base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                       this, std::move(loaded_callback),
                       std::move(loaded_policies), false /* load_success */));
    return;
  }

  while (smt.Step()) {
    // Reconstitute a NEL policy from the fields stored in the database.
    NetworkErrorLoggingService::NELPolicy policy;
    policy.origin = url::Origin::CreateFromNormalizedTuple(
        /* origin_scheme = */ smt.ColumnString(0),
        /* origin_host = */ smt.ColumnString(1),
        /* origin_port = */ smt.ColumnInt(2));
    if (!policy.received_ip_address.AssignFromIPLiteral(smt.ColumnString(3)))
      policy.received_ip_address = IPAddress();
    policy.report_to = smt.ColumnString(4);
    policy.expires = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(smt.ColumnInt64(5)));
    policy.success_fraction = smt.ColumnDouble(6);
    policy.failure_fraction = smt.ColumnDouble(7);
    policy.include_subdomains = smt.ColumnBool(8);
    policy.last_used = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(smt.ColumnInt64(9)));

    loaded_policies.push_back(std::move(policy));
  }

  PostClientTask(
      FROM_HERE,
      base::BindOnce(&Backend::CompleteLoadNELPoliciesAndNotifyInForeground,
                     this, std::move(loaded_callback),
                     std::move(loaded_policies), true /* load_success */));
}

void SQLitePersistentReportingAndNELStore::Backend::
    CompleteLoadNELPoliciesAndNotifyInForeground(
        NELPoliciesLoadedCallback loaded_callback,
        std::vector<NetworkErrorLoggingService::NELPolicy> loaded_policies,
        bool load_success) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  if (load_success) {
    // TODO(chlily): report metrics
  } else {
    DCHECK(loaded_policies.empty());
  }

  std::move(loaded_callback).Run(std::move(loaded_policies));
}

void SQLitePersistentReportingAndNELStore::Backend::
    LoadReportingClientsAndNotifyInBackground(
        ReportingClientsLoadedCallback loaded_callback) {
  DCHECK(background_task_runner()->RunsTasksInCurrentSequence());

  std::vector<ReportingEndpoint> loaded_endpoints;
  std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups;
  if (!InitializeDatabase()) {
    PostClientTask(
        FROM_HERE,
        base::BindOnce(
            &Backend::CompleteLoadReportingClientsAndNotifyInForeground, this,
            std::move(loaded_callback), std::move(loaded_endpoints),
            std::move(loaded_endpoint_groups), false /* load_success */));
    return;
  }

  sql::Statement endpoints_smt(db()->GetUniqueStatement(
      "SELECT origin_scheme, origin_host, origin_port, group_name, "
      "url, priority, weight FROM reporting_endpoints"));
  sql::Statement endpoint_groups_smt(db()->GetUniqueStatement(
      "SELECT origin_scheme, origin_host, origin_port, group_name, "
      "is_include_subdomains, expires_us_since_epoch, "
      "last_access_us_since_epoch FROM reporting_endpoint_groups"));
  if (!endpoints_smt.is_valid() || !endpoint_groups_smt.is_valid()) {
    Reset();
    PostClientTask(
        FROM_HERE,
        base::BindOnce(
            &Backend::CompleteLoadReportingClientsAndNotifyInForeground, this,
            std::move(loaded_callback), std::move(loaded_endpoints),
            std::move(loaded_endpoint_groups), false /* load_success */));
    return;
  }

  while (endpoints_smt.Step()) {
    // Reconstitute a ReportingEndpoint from the fields stored in the database.
    url::Origin origin = url::Origin::CreateFromNormalizedTuple(
        /* origin_scheme = */ endpoints_smt.ColumnString(0),
        /* origin_host = */ endpoints_smt.ColumnString(1),
        /* origin_port = */ endpoints_smt.ColumnInt(2));
    std::string group_name = endpoints_smt.ColumnString(3);
    ReportingEndpoint::EndpointInfo endpoint_info;
    endpoint_info.url = GURL(endpoints_smt.ColumnString(4));
    endpoint_info.priority = endpoints_smt.ColumnInt(5);
    endpoint_info.weight = endpoints_smt.ColumnInt(6);

    loaded_endpoints.emplace_back(std::move(origin), std::move(group_name),
                                  std::move(endpoint_info));
  }

  while (endpoint_groups_smt.Step()) {
    // Reconstitute a CachedReportingEndpointGroup from the fields stored in the
    // database.
    url::Origin origin = url::Origin::CreateFromNormalizedTuple(
        /* origin_scheme = */ endpoint_groups_smt.ColumnString(0),
        /* origin_host = */ endpoint_groups_smt.ColumnString(1),
        /* origin_port = */ endpoint_groups_smt.ColumnInt(2));
    std::string group_name = endpoint_groups_smt.ColumnString(3);
    OriginSubdomains include_subdomains = endpoint_groups_smt.ColumnBool(4)
                                              ? OriginSubdomains::INCLUDE
                                              : OriginSubdomains::EXCLUDE;
    base::Time expires = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(endpoint_groups_smt.ColumnInt64(5)));
    base::Time last_used = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(endpoint_groups_smt.ColumnInt64(6)));

    loaded_endpoint_groups.emplace_back(std::move(origin),
                                        std::move(group_name),
                                        include_subdomains, expires, last_used);
  }

  PostClientTask(
      FROM_HERE,
      base::BindOnce(
          &Backend::CompleteLoadReportingClientsAndNotifyInForeground, this,
          std::move(loaded_callback), std::move(loaded_endpoints),
          std::move(loaded_endpoint_groups), true /* load_success */));
}

void SQLitePersistentReportingAndNELStore::Backend::
    CompleteLoadReportingClientsAndNotifyInForeground(
        ReportingClientsLoadedCallback loaded_callback,
        std::vector<ReportingEndpoint> loaded_endpoints,
        std::vector<CachedReportingEndpointGroup> loaded_endpoint_groups,
        bool load_success) {
  DCHECK(client_task_runner()->RunsTasksInCurrentSequence());

  if (load_success) {
    // TODO(chlily): report metrics
  } else {
    DCHECK(loaded_endpoints.empty());
    DCHECK(loaded_endpoint_groups.empty());
  }

  std::move(loaded_callback)
      .Run(std::move(loaded_endpoints), std::move(loaded_endpoint_groups));
}

SQLitePersistentReportingAndNELStore::SQLitePersistentReportingAndNELStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : backend_(new Backend(path, client_task_runner, background_task_runner)),
      weak_factory_(this) {}

SQLitePersistentReportingAndNELStore::~SQLitePersistentReportingAndNELStore() {
  backend_->Close();
}

void SQLitePersistentReportingAndNELStore::LoadNELPolicies(
    NELPoliciesLoadedCallback loaded_callback) {
  DCHECK(!loaded_callback.is_null());
  backend_->LoadNELPolicies(base::BindOnce(
      &SQLitePersistentReportingAndNELStore::CompleteLoadNELPolicies,
      weak_factory_.GetWeakPtr(), std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::AddNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->AddNELPolicy(policy);
}

void SQLitePersistentReportingAndNELStore::UpdateNELPolicyAccessTime(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->UpdateNELPolicyAccessTime(policy);
}

void SQLitePersistentReportingAndNELStore::DeleteNELPolicy(
    const NetworkErrorLoggingService::NELPolicy& policy) {
  backend_->DeleteNELPolicy(policy);
}

void SQLitePersistentReportingAndNELStore::LoadReportingClients(
    ReportingClientsLoadedCallback loaded_callback) {
  DCHECK(!loaded_callback.is_null());
  backend_->LoadReportingClients(base::BindOnce(
      &SQLitePersistentReportingAndNELStore::CompleteLoadReportingClients,
      weak_factory_.GetWeakPtr(), std::move(loaded_callback)));
}

void SQLitePersistentReportingAndNELStore::AddReportingEndpoint(
    const ReportingEndpoint& endpoint) {
  backend_->AddReportingEndpoint(endpoint);
}

void SQLitePersistentReportingAndNELStore::AddReportingEndpointGroup(
    const CachedReportingEndpointGroup& group) {
  backend_->AddReportingEndpointGroup(group);
}

void SQLitePersistentReportingAndNELStore::
    UpdateReportingEndpointGroupAccessTime(
        const CachedReportingEndpointGroup& group) {
  backend_->UpdateReportingEndpointGroupAccessTime(group);
}

void SQLitePersistentReportingAndNELStore::UpdateReportingEndpointDetails(
    const ReportingEndpoint& endpoint) {
  backend_->UpdateReportingEndpointDetails(endpoint);
}

void SQLitePersistentReportingAndNELStore::UpdateReportingEndpointGroupDetails(
    const CachedReportingEndpointGroup& group) {
  backend_->UpdateReportingEndpointGroupDetails(group);
}

void SQLitePersistentReportingAndNELStore::DeleteReportingEndpoint(
    const ReportingEndpoint& endpoint) {
  backend_->DeleteReportingEndpoint(endpoint);
}

void SQLitePersistentReportingAndNELStore::DeleteReportingEndpointGroup(
    const CachedReportingEndpointGroup& group) {
  backend_->DeleteReportingEndpointGroup(group);
}

void SQLitePersistentReportingAndNELStore::Flush() {
  backend_->Flush(base::DoNothing());
}

size_t SQLitePersistentReportingAndNELStore::GetQueueLengthForTesting() const {
  return backend_->GetQueueLengthForTesting();
}

void SQLitePersistentReportingAndNELStore::CompleteLoadNELPolicies(
    NELPoliciesLoadedCallback callback,
    std::vector<NetworkErrorLoggingService::NELPolicy> policies) {
  std::move(callback).Run(std::move(policies));
}

void SQLitePersistentReportingAndNELStore::CompleteLoadReportingClients(
    ReportingClientsLoadedCallback callback,
    std::vector<ReportingEndpoint> endpoints,
    std::vector<CachedReportingEndpointGroup> endpoint_groups) {
  std::move(callback).Run(std::move(endpoints), std::move(endpoint_groups));
}

}  // namespace net
