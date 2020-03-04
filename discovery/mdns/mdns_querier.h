// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_QUERIER_H_
#define DISCOVERY_MDNS_MDNS_QUERIER_H_

#include <map>

#include "discovery/common/config.h"
#include "discovery/mdns/mdns_receiver.h"
#include "discovery/mdns/mdns_record_changed_callback.h"
#include "discovery/mdns/mdns_records.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

class MdnsRandom;
class MdnsSender;
class MdnsQuestionTracker;
class MdnsRecordTracker;
class ReportingClient;

class MdnsQuerier : public MdnsReceiver::ResponseClient {
 public:
  MdnsQuerier(MdnsSender* sender,
              MdnsReceiver* receiver,
              TaskRunner* task_runner,
              ClockNowFunctionPtr now_function,
              MdnsRandom* random_delay,
              ReportingClient* reporting_client,
              Config config);
  MdnsQuerier(const MdnsQuerier& other) = delete;
  MdnsQuerier(MdnsQuerier&& other) noexcept = delete;
  MdnsQuerier& operator=(const MdnsQuerier& other) = delete;
  MdnsQuerier& operator=(MdnsQuerier&& other) noexcept = delete;
  ~MdnsQuerier() override;

  // Starts an mDNS query with the given name, DNS type, and DNS class.  Updated
  // records are passed to |callback|.  The caller must ensure |callback|
  // remains alive while it is registered with a query.
  // NOTE: NSEC records cannot be queried for.
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback);

  // Stops an mDNS query with the given name, DNS type, and DNS class.
  // |callback| must be the same callback pointer that was previously passed to
  // StartQuery.
  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback);

  // Re-initializes the process of service discovery for the provided domain
  // name. All ongoing queries for this domain are restarted and any previously
  // received query results are discarded.
  void ReinitializeQueries(const DomainName& name);

 private:
  struct CallbackInfo {
    MdnsRecordChangedCallback* const callback;
    const DnsType dns_type;
    const DnsClass dns_class;
  };

  friend class MdnsQuerierTest;

  // MdnsReceiver::ResponseClient overrides.
  void OnMessageReceived(const MdnsMessage& message) override;

  // Expires the record tracker provided. This callback is passed to owned
  // MdnsRecordTracker instances in |records_|.
  void OnRecordExpired(MdnsRecordTracker* tracker, const MdnsRecord& record);

  // Determines whether a record received by this querier should be processed
  // or dropped.
  bool ShouldAnswerRecordBeProcessed(const MdnsRecord& answer);

  // Processes any record update, calling into the below methods as needed.
  void ProcessRecord(const MdnsRecord& records);

  // Processes a shared record update as a record of type |type|.
  void ProcessSharedRecord(const MdnsRecord& record, DnsType type);

  // Processes a unique record update as a record of type |type|.
  void ProcessUniqueRecord(const MdnsRecord& record, DnsType type);

  // Called when exactly one tracker is associated with a provided key.
  // Determines the type of update being executed by this update call, then
  // fires the appropriate callback.
  void ProcessSinglyTrackedUniqueRecord(const MdnsRecord& record,
                                        MdnsRecordTracker* tracker);

  // Called when multiple records are associated with the same key. Expire all
  // record with non-matching RDATA. Update the record with the matching RDATA
  // if it exists, otherwise insert a new record.
  void ProcessMultiTrackedUniqueRecord(const MdnsRecord& record,
                                       DnsType dns_type);

  // Calls all callbacks associated with the provided record.
  void ProcessCallbacks(const MdnsRecord& record, RecordChangedEvent event);

  // Begins tracking the provided question.
  void AddQuestion(const MdnsQuestion& question);

  // Begins tracking the provided record.
  void AddRecord(const MdnsRecord& record, DnsType type);

  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* const task_runner_;
  const ClockNowFunctionPtr now_function_;
  MdnsRandom* const random_delay_;
  ReportingClient* reporting_client_;
  Config config_;

  // A collection of active question trackers, each is uniquely identified by
  // domain name, DNS record type, and DNS record class. Multimap key is domain
  // name only to allow easy support for wildcard processing for DNS record type
  // and class. MdnsQuestionTracker instances are stored as unique_ptr so they
  // are not moved around in memory when the collection is modified. This allows
  // passing a pointer to MdnsQuestionTracker to a task running on the
  // TaskRunner.
  std::multimap<DomainName, std::unique_ptr<MdnsQuestionTracker>> questions_;

  // A collection of active known record trackers, each is identified by domain
  // name, DNS record type, and DNS record class. Multimap key is domain name
  // only to allow easy support for wildcard processing for DNS record type and
  // class and allow storing shared records that differ only in RDATA.
  // MdnsRecordTracker instances are stored as unique_ptr so they are not moved
  // around in memory when the collection is modified. This allows passing a
  // pointer to MdnsQuestionTracker to a task running on the TaskRunner.
  std::multimap<DomainName, std::unique_ptr<MdnsRecordTracker>> records_;

  // A collection of callbacks passed to StartQuery method. Each is identified
  // by domain name, DNS record type, and DNS record class, but there can be
  // more than one callback for a particular query. Multimap key is domain name
  // only to allow easy matching of records against callbacks that have wildcard
  // DNS class and/or DNS type.
  std::multimap<DomainName, CallbackInfo> callbacks_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_QUERIER_H_
