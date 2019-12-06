// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_PUBLISHER_H_
#define DISCOVERY_MDNS_MDNS_PUBLISHER_H_

#include <map>

#include "discovery/mdns/mdns_records.h"
#include "discovery/mdns/mdns_responder.h"

namespace openscreen {

class TaskRunner;

namespace discovery {

// This class is responsible for both tracking what records have been registered
// to mDNS as well as publishing new mDNS Records to the network.
// When a new record is published, it will be announced 8 times, starting at an
// interval of 1 second, with the interval doubling each successive
// announcement. This same announcement process is followed when an existing
// record is updated. When it is removed, a Goodbye message must be sent if the
// record is unique.
// Prior to publishing a record, the domain name for this service instance must
// be claimed using the ClaimExclusiveOwnership() function. This function probes
// the network to determine whether the chosen name exists, modifying the
// chosen name as described in RFC 6762 if a collision is found.
// TODO(rwkeane): Add ClaimExclusiveOwnership() API for claiming a DomainName as
// described in RFC 6762 Section 8.1's probing phase.
class MdnsPublisher : public MdnsResponder::RecordHandler {
 public:
  // |querier|, |sender|, |task_runner|, and |random_delay| must all persist for
  // the duration of this object's lifetime
  MdnsPublisher(MdnsQuerier* querier,
                MdnsSender* sender,
                TaskRunner* task_runner,
                MdnsRandom* random_delay);
  ~MdnsPublisher() override;

  // Registers a new mDNS record for advertisement by this service. For A, AAAA,
  // SRV, and TXT records, the domain name must have already been claimed by the
  // ClaimExclusiveOwnership() method and for PTR records the name being pointed
  // to must have been claimed in the same fashion, but the domain name in the
  // top-level MdnsRecord entity does not.
  Error RegisterRecord(const MdnsRecord& record);

  // Updates the existing record with name matching the name of the new record.
  // NOTE: This method is not valid for PTR records.
  Error UpdateRegisteredRecord(const MdnsRecord& old_record,
                               const MdnsRecord& new_record);

  // Stops advertising the provided record. If no more records with the provided
  // name are bing advertised after this call's completion, then ownership of
  // the name is released.
  Error UnregisterRecord(const MdnsRecord& record);

  // Returns the total number of records currently registered;
  size_t GetRecordCount() const;

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsPublisher);

 private:
  friend class MdnsPublisherTesting;

  // Registers a new MdnsRecord. Only valid for the annotated types.
  Error RegisterPtrRecord(const MdnsRecord& record);
  Error RegisterNonPtrRecord(const MdnsRecord& record);

  // Unregisters a previously existing MdnsRecord. Only valid for the annotated
  // types.
  Error UnregisterPtrRecord(const MdnsRecord& record);
  Error UnregisterNonPtrRecord(const MdnsRecord& record);

  // Removes the provided record from the |records_| map, returning an error if
  // it does not exist.
  Error RemoveNonPtrRecord(const MdnsRecord& record);

  // MdnsResponder::RecordHandler overrides.
  bool IsExclusiveOwner(const DomainName& name) override;
  bool HasRecords(const DomainName& name,
                  DnsType type,
                  DnsClass clazz) override;
  std::vector<MdnsRecord::ConstRef> GetRecords(const DomainName& name,
                                               DnsType type,
                                               DnsClass clazz) override;

  MdnsQuerier* const querier_;
  MdnsSender* const sender_;
  TaskRunner* const task_runner_;
  MdnsRandom* const random_delay_;

  // Stores non-PTR mDNS records that have been published. The keys here are the
  // set of DomainNames for which this service is the exclusive owner, and the
  // values are all records which have that name. For that reason, a key is
  // present in this map if and only if this service instance is the exclusive
  // owner of that domain name.
  std::map<DomainName, std::vector<MdnsRecord>> records_;

  // Stores all PTR mDNS records. These are stored separately from the above
  // records because the DomainName parameter here is not unique to this
  // service instance, so the presence of a record for a given PTR domain is
  // different from that of a non-PTR domain.
  // NOTE: This storage format assumes that only one PTR record for a given
  // service type will be registered per service instance.
  std::map<DomainName, MdnsRecord> ptr_records_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_PUBLISHER_H_
