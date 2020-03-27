// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_MDNS_RESPONDER_H_
#define DISCOVERY_MDNS_MDNS_RESPONDER_H_

#include <vector>

#include "discovery/mdns/mdns_records.h"
#include "platform/api/time.h"
#include "platform/base/macros.h"

namespace openscreen {

struct IPEndpoint;
class TaskRunner;

namespace discovery {

class MdnsMessage;
class MdnsProbeManager;
class MdnsRandom;
class MdnsReceiver;
class MdnsRecordChangedCallback;
class MdnsSender;
class MdnsQuerier;

// This class is responsible for responding to any incoming mDNS Queries
// received via the OnMessageReceived() method. When responding, the generated
// MdnsMessage will contain the requested record(s) in the answers section, or
// an NSEC record to specify that the requested record was not found in the case
// of a query with DnsType aside from ANY. In the case where records are found,
// the additional records field may be populated with additional records, as
// specified in RFCs 6762 and 6763.
// TODO(rwkeane): Handle known answers, and waiting when the truncated (TC) bit
// is set.
class MdnsResponder {
 public:
  // Class to handle querying for existing records.
  class RecordHandler {
   public:
    virtual ~RecordHandler();

    // Returns whether this service has one or more records matching the
    // provided name, type, and class.
    virtual bool HasRecords(const DomainName& name,
                            DnsType type,
                            DnsClass clazz) = 0;

    // Returns all records owned by this service with name, type, and class
    // matching the provided values.
    virtual std::vector<MdnsRecord::ConstRef> GetRecords(const DomainName& name,
                                                         DnsType type,
                                                         DnsClass clazz) = 0;

    // Enumerates all PTR records owned by this service.
    virtual std::vector<MdnsRecord::ConstRef> GetPtrRecords(DnsClass clazz) = 0;
  };

  // |record_handler|, |sender|, |receiver|, |task_runner|, and |random_delay|
  // are expected to persist for the duration of this instance's lifetime.
  MdnsResponder(RecordHandler* record_handler,
                MdnsProbeManager* ownership_handler,
                MdnsSender* sender,
                MdnsReceiver* receiver,
                TaskRunner* task_runner,
                MdnsRandom* random_delay);
  ~MdnsResponder();

  OSP_DISALLOW_COPY_AND_ASSIGN(MdnsResponder);

 private:
  void OnMessageReceived(const MdnsMessage& message, const IPEndpoint& src);

  void SendResponse(const MdnsQuestion& question,
                    const std::vector<MdnsRecord>& known_answers,
                    std::function<void(const MdnsMessage&)> send_response,
                    bool is_exclusive_owner);

  RecordHandler* const record_handler_;
  MdnsProbeManager* const ownership_handler_;
  MdnsSender* const sender_;
  MdnsReceiver* const receiver_;
  TaskRunner* const task_runner_;
  MdnsRandom* const random_delay_;

  friend class MdnsResponderTest;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_MDNS_RESPONDER_H_
