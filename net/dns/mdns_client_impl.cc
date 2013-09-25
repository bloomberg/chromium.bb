// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mdns_client_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "base/time/default_clock.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/rand_callback.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/record_rdata.h"
#include "net/udp/datagram_socket.h"

// TODO(gene): Remove this temporary method of disabling NSEC support once it
// becomes clear whether this feature should be
// supported. http://crbug.com/255232
#define ENABLE_NSEC

namespace net {

namespace {
const unsigned MDnsTransactionTimeoutSeconds = 3;
}

MDnsConnection::SocketHandler::SocketHandler(
    MDnsConnection* connection, const IPEndPoint& multicast_addr,
    MDnsConnection::SocketFactory* socket_factory)
    : socket_(socket_factory->CreateSocket()), connection_(connection),
      response_(new DnsResponse(dns_protocol::kMaxMulticastSize)),
      multicast_addr_(multicast_addr) {
}

MDnsConnection::SocketHandler::~SocketHandler() {
}

int MDnsConnection::SocketHandler::Start() {
  return DoLoop(0);
}

int MDnsConnection::SocketHandler::DoLoop(int rv) {
  do {
    if (rv > 0)
      connection_->OnDatagramReceived(response_.get(), recv_addr_, rv);

    rv = socket_->RecvFrom(
        response_->io_buffer(),
        response_->io_buffer()->size(),
        &recv_addr_,
        base::Bind(&MDnsConnection::SocketHandler::OnDatagramReceived,
                   base::Unretained(this)));
  } while (rv > 0);

  if (rv != ERR_IO_PENDING)
    return rv;

  return OK;
}

void MDnsConnection::SocketHandler::OnDatagramReceived(int rv) {
  if (rv >= OK)
    rv = DoLoop(rv);

  if (rv != OK)
    connection_->OnError(this, rv);
}

int MDnsConnection::SocketHandler::Send(IOBuffer* buffer, unsigned size) {
  return socket_->SendTo(
      buffer, size, multicast_addr_,
      base::Bind(&MDnsConnection::SocketHandler::SendDone,
                 base::Unretained(this) ));
}

void MDnsConnection::SocketHandler::SendDone(int rv) {
  // TODO(noamsml): Retry logic.
}

int MDnsConnection::SocketHandler::Bind() {
  IPAddressNumber address_any(multicast_addr_.address().size());

  IPEndPoint bind_endpoint(address_any, multicast_addr_.port());

  socket_->AllowAddressReuse();
  int rv = socket_->Listen(bind_endpoint);

  if (rv < OK) return rv;

  socket_->SetMulticastLoopbackMode(false);

  return socket_->JoinGroup(multicast_addr_.address());
}

MDnsConnection::MDnsConnection(MDnsConnection::Delegate* delegate) :
      delegate_(delegate) {
}

MDnsConnection::~MDnsConnection() {
}

bool MDnsConnection::Init(MDnsConnection::SocketFactory* socket_factory) {
  // TODO(vitalybuka): crbug.com/297690 Make socket_factory return list
  // of initialized sockets.
  socket_handlers_.push_back(
      new SocketHandler(this, GetMDnsIPEndPoint(ADDRESS_FAMILY_IPV4),
                        socket_factory));
  socket_handlers_.push_back(
      new SocketHandler(this, GetMDnsIPEndPoint(ADDRESS_FAMILY_IPV6),
                        socket_factory));

  for (size_t i = 0; i < socket_handlers_.size();) {
    if (socket_handlers_[i]->Bind() != OK) {
      socket_handlers_.erase(socket_handlers_.begin() + i);
    } else {
      ++i;
    }
  }

  // All unbound sockets need to be bound before processing untrusted input.
  // This is done for security reasons, so that an attacker can't get an unbound
  // socket.
  for (size_t i = 0; i < socket_handlers_.size();) {
    if (socket_handlers_[i]->Start() != OK) {
      socket_handlers_.erase(socket_handlers_.begin() + i);
    } else {
      ++i;
    }
  }
  return !socket_handlers_.empty();
}

bool MDnsConnection::Send(IOBuffer* buffer, unsigned size) {
  bool success = false;
  for (size_t i = 0; i < socket_handlers_.size(); ++i) {
    int rv = socket_handlers_[i]->Send(buffer, size);
    success = success || (rv >= OK || rv == ERR_IO_PENDING);
  }
  return success;
}

void MDnsConnection::OnError(SocketHandler* loop,
                             int error) {
  // TODO(noamsml): Specific handling of intermittent errors that can be handled
  // in the connection.
  delegate_->OnConnectionError(error);
}

void MDnsConnection::OnDatagramReceived(
    DnsResponse* response,
    const IPEndPoint& recv_addr,
    int bytes_read) {
  // TODO(noamsml): More sophisticated error handling.
  DCHECK_GT(bytes_read, 0);
  delegate_->HandlePacket(response, bytes_read);
}

class MDnsConnectionSocketFactoryImpl
    : public MDnsConnection::SocketFactory {
 public:
  MDnsConnectionSocketFactoryImpl();
  virtual ~MDnsConnectionSocketFactoryImpl();

  virtual scoped_ptr<DatagramServerSocket> CreateSocket() OVERRIDE;
};

MDnsConnectionSocketFactoryImpl::MDnsConnectionSocketFactoryImpl() {
}

MDnsConnectionSocketFactoryImpl::~MDnsConnectionSocketFactoryImpl() {
}

scoped_ptr<DatagramServerSocket>
MDnsConnectionSocketFactoryImpl::CreateSocket() {
  return scoped_ptr<DatagramServerSocket>(new UDPServerSocket(
      NULL, NetLog::Source()));
}

// static
scoped_ptr<MDnsConnection::SocketFactory>
MDnsConnection::SocketFactory::CreateDefault() {
  return scoped_ptr<MDnsConnection::SocketFactory>(
      new MDnsConnectionSocketFactoryImpl);
}

MDnsClientImpl::Core::Core(MDnsClientImpl* client)
    : client_(client), connection_(new MDnsConnection(this)) {
}

MDnsClientImpl::Core::~Core() {
  STLDeleteValues(&listeners_);
}

bool MDnsClientImpl::Core::Init(MDnsConnection::SocketFactory* socket_factory) {
  return connection_->Init(socket_factory);
}

bool MDnsClientImpl::Core::SendQuery(uint16 rrtype, std::string name) {
  std::string name_dns;
  if (!DNSDomainFromDot(name, &name_dns))
    return false;

  DnsQuery query(0, name_dns, rrtype);
  query.set_flags(0);  // Remove the RD flag from the query. It is unneeded.

  return connection_->Send(query.io_buffer(), query.io_buffer()->size());
}

void MDnsClientImpl::Core::HandlePacket(DnsResponse* response,
                                        int bytes_read) {
  unsigned offset;
  // Note: We store cache keys rather than record pointers to avoid
  // erroneous behavior in case a packet contains multiple exclusive
  // records with the same type and name.
  std::map<MDnsCache::Key, MDnsListener::UpdateType> update_keys;

  if (!response->InitParseWithoutQuery(bytes_read)) {
    LOG(WARNING) << "Could not understand an mDNS packet.";
    return;  // Message is unreadable.
  }

  // TODO(noamsml): duplicate query suppression.
  if (!(response->flags() & dns_protocol::kFlagResponse))
    return;  // Message is a query. ignore it.

  DnsRecordParser parser = response->Parser();
  unsigned answer_count = response->answer_count() +
      response->additional_answer_count();

  for (unsigned i = 0; i < answer_count; i++) {
    offset = parser.GetOffset();
    scoped_ptr<const RecordParsed> record = RecordParsed::CreateFrom(
        &parser, base::Time::Now());

    if (!record) {
      LOG(WARNING) << "Could not understand an mDNS record.";

      if (offset == parser.GetOffset()) {
        LOG(WARNING) << "Abandoned parsing the rest of the packet.";
        return;  // The parser did not advance, abort reading the packet.
      } else {
        continue;  // We may be able to extract other records from the packet.
      }
    }

    if ((record->klass() & dns_protocol::kMDnsClassMask) !=
        dns_protocol::kClassIN) {
      LOG(WARNING) << "Received an mDNS record with non-IN class. Ignoring.";
      continue;  // Ignore all records not in the IN class.
    }

    MDnsCache::Key update_key = MDnsCache::Key::CreateFor(record.get());
    MDnsCache::UpdateType update = cache_.UpdateDnsRecord(record.Pass());

    // Cleanup time may have changed.
    ScheduleCleanup(cache_.next_expiration());

    if (update != MDnsCache::NoChange) {
      MDnsListener::UpdateType update_external;

      switch (update) {
        case MDnsCache::RecordAdded:
          update_external = MDnsListener::RECORD_ADDED;
          break;
        case MDnsCache::RecordChanged:
          update_external = MDnsListener::RECORD_CHANGED;
          break;
        case MDnsCache::NoChange:
        default:
          NOTREACHED();
          // Dummy assignment to suppress compiler warning.
          update_external = MDnsListener::RECORD_CHANGED;
          break;
      }

      update_keys.insert(std::make_pair(update_key, update_external));
    }
  }

  for (std::map<MDnsCache::Key, MDnsListener::UpdateType>::iterator i =
           update_keys.begin(); i != update_keys.end(); i++) {
    const RecordParsed* record = cache_.LookupKey(i->first);
    if (!record)
      continue;

    if (record->type() == dns_protocol::kTypeNSEC) {
#if defined(ENABLE_NSEC)
      NotifyNsecRecord(record);
#endif
    } else {
      AlertListeners(i->second, ListenerKey(record->name(), record->type()),
                     record);
    }
  }
}

void MDnsClientImpl::Core::NotifyNsecRecord(const RecordParsed* record) {
  DCHECK_EQ(dns_protocol::kTypeNSEC, record->type());
  const NsecRecordRdata* rdata = record->rdata<NsecRecordRdata>();
  DCHECK(rdata);

  // Remove all cached records matching the nonexistent RR types.
  std::vector<const RecordParsed*> records_to_remove;

  cache_.FindDnsRecords(0, record->name(), &records_to_remove,
                        base::Time::Now());

  for (std::vector<const RecordParsed*>::iterator i = records_to_remove.begin();
       i != records_to_remove.end(); i++) {
    if ((*i)->type() == dns_protocol::kTypeNSEC)
      continue;
    if (!rdata->GetBit((*i)->type())) {
      scoped_ptr<const RecordParsed> record_removed = cache_.RemoveRecord((*i));
      DCHECK(record_removed);
      OnRecordRemoved(record_removed.get());
    }
  }

  // Alert all listeners waiting for the nonexistent RR types.
  ListenerMap::iterator i =
      listeners_.upper_bound(ListenerKey(record->name(), 0));
  for (; i != listeners_.end() && i->first.first == record->name(); i++) {
    if (!rdata->GetBit(i->first.second)) {
      FOR_EACH_OBSERVER(MDnsListenerImpl, *i->second, AlertNsecRecord());
    }
  }
}

void MDnsClientImpl::Core::OnConnectionError(int error) {
  // TODO(noamsml): On connection error, recreate connection and flush cache.
}

void MDnsClientImpl::Core::AlertListeners(
    MDnsListener::UpdateType update_type,
    const ListenerKey& key,
    const RecordParsed* record) {
  ListenerMap::iterator listener_map_iterator = listeners_.find(key);
  if (listener_map_iterator == listeners_.end()) return;

  FOR_EACH_OBSERVER(MDnsListenerImpl, *listener_map_iterator->second,
                    AlertDelegate(update_type, record));
}

void MDnsClientImpl::Core::AddListener(
    MDnsListenerImpl* listener) {
  ListenerKey key(listener->GetName(), listener->GetType());
  std::pair<ListenerMap::iterator, bool> observer_insert_result =
      listeners_.insert(
          make_pair(key, static_cast<ObserverList<MDnsListenerImpl>*>(NULL)));

  // If an equivalent key does not exist, actually create the observer list.
  if (observer_insert_result.second)
    observer_insert_result.first->second = new ObserverList<MDnsListenerImpl>();

  ObserverList<MDnsListenerImpl>* observer_list =
      observer_insert_result.first->second;

  observer_list->AddObserver(listener);
}

void MDnsClientImpl::Core::RemoveListener(MDnsListenerImpl* listener) {
  ListenerKey key(listener->GetName(), listener->GetType());
  ListenerMap::iterator observer_list_iterator = listeners_.find(key);

  DCHECK(observer_list_iterator != listeners_.end());
  DCHECK(observer_list_iterator->second->HasObserver(listener));

  observer_list_iterator->second->RemoveObserver(listener);

  // Remove the observer list from the map if it is empty
  if (!observer_list_iterator->second->might_have_observers()) {
    // Schedule the actual removal for later in case the listener removal
    // happens while iterating over the observer list.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(
            &MDnsClientImpl::Core::CleanupObserverList, AsWeakPtr(), key));
  }
}

void MDnsClientImpl::Core::CleanupObserverList(const ListenerKey& key) {
  ListenerMap::iterator found = listeners_.find(key);
  if (found != listeners_.end() && !found->second->might_have_observers()) {
    delete found->second;
    listeners_.erase(found);
  }
}

void MDnsClientImpl::Core::ScheduleCleanup(base::Time cleanup) {
  // Cleanup is already scheduled, no need to do anything.
  if (cleanup == scheduled_cleanup_) return;
  scheduled_cleanup_ = cleanup;

  // This cancels the previously scheduled cleanup.
  cleanup_callback_.Reset(base::Bind(
      &MDnsClientImpl::Core::DoCleanup, base::Unretained(this)));

  // If |cleanup| is empty, then no cleanup necessary.
  if (cleanup != base::Time()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        cleanup_callback_.callback(),
        cleanup - base::Time::Now());
  }
}

void MDnsClientImpl::Core::DoCleanup() {
  cache_.CleanupRecords(base::Time::Now(), base::Bind(
      &MDnsClientImpl::Core::OnRecordRemoved, base::Unretained(this)));

  ScheduleCleanup(cache_.next_expiration());
}

void MDnsClientImpl::Core::OnRecordRemoved(
    const RecordParsed* record) {
  AlertListeners(MDnsListener::RECORD_REMOVED,
                 ListenerKey(record->name(), record->type()), record);
}

void MDnsClientImpl::Core::QueryCache(
    uint16 rrtype, const std::string& name,
    std::vector<const RecordParsed*>* records) const {
  cache_.FindDnsRecords(rrtype, name, records, base::Time::Now());
}

MDnsClientImpl::MDnsClientImpl(
    scoped_ptr<MDnsConnection::SocketFactory> socket_factory)
    : socket_factory_(socket_factory.Pass()) {
}

MDnsClientImpl::~MDnsClientImpl() {
}

bool MDnsClientImpl::StartListening() {
  DCHECK(!core_.get());
  core_.reset(new Core(this));
  if (!core_->Init(socket_factory_.get())) {
    core_.reset();
    return false;
  }
  return true;
}

void MDnsClientImpl::StopListening() {
  core_.reset();
}

bool MDnsClientImpl::IsListening() const {
  return core_.get() != NULL;
}

scoped_ptr<MDnsListener> MDnsClientImpl::CreateListener(
    uint16 rrtype,
    const std::string& name,
    MDnsListener::Delegate* delegate) {
  return scoped_ptr<net::MDnsListener>(
      new MDnsListenerImpl(rrtype, name, delegate, this));
}

scoped_ptr<MDnsTransaction> MDnsClientImpl::CreateTransaction(
    uint16 rrtype,
    const std::string& name,
    int flags,
    const MDnsTransaction::ResultCallback& callback) {
  return scoped_ptr<MDnsTransaction>(
      new MDnsTransactionImpl(rrtype, name, flags, callback, this));
}

MDnsListenerImpl::MDnsListenerImpl(
    uint16 rrtype,
    const std::string& name,
    MDnsListener::Delegate* delegate,
    MDnsClientImpl* client)
    : rrtype_(rrtype), name_(name), client_(client), delegate_(delegate),
      started_(false) {
}

bool MDnsListenerImpl::Start() {
  DCHECK(!started_);

  started_ = true;

  DCHECK(client_->core());
  client_->core()->AddListener(this);

  return true;
}

MDnsListenerImpl::~MDnsListenerImpl() {
  if (started_) {
    DCHECK(client_->core());
    client_->core()->RemoveListener(this);
  }
}

const std::string& MDnsListenerImpl::GetName() const {
  return name_;
}

uint16 MDnsListenerImpl::GetType() const {
  return rrtype_;
}

void MDnsListenerImpl::AlertDelegate(MDnsListener::UpdateType update_type,
                                     const RecordParsed* record) {
  DCHECK(started_);
  delegate_->OnRecordUpdate(update_type, record);
}

void MDnsListenerImpl::AlertNsecRecord() {
  DCHECK(started_);
  delegate_->OnNsecRecord(name_, rrtype_);
}

MDnsTransactionImpl::MDnsTransactionImpl(
    uint16 rrtype,
    const std::string& name,
    int flags,
    const MDnsTransaction::ResultCallback& callback,
    MDnsClientImpl* client)
    : rrtype_(rrtype), name_(name), callback_(callback), client_(client),
      started_(false), flags_(flags) {
  DCHECK((flags_ & MDnsTransaction::FLAG_MASK) == flags_);
  DCHECK(flags_ & MDnsTransaction::QUERY_CACHE ||
         flags_ & MDnsTransaction::QUERY_NETWORK);
}

MDnsTransactionImpl::~MDnsTransactionImpl() {
  timeout_.Cancel();
}

bool MDnsTransactionImpl::Start() {
  DCHECK(!started_);
  started_ = true;

  base::WeakPtr<MDnsTransactionImpl> weak_this = AsWeakPtr();
  if (flags_ & MDnsTransaction::QUERY_CACHE) {
    ServeRecordsFromCache();

    if (!weak_this || !is_active()) return true;
  }

  if (flags_ & MDnsTransaction::QUERY_NETWORK) {
    return QueryAndListen();
  }

  // If this is a cache only query, signal that the transaction is over
  // immediately.
  SignalTransactionOver();
  return true;
}

const std::string& MDnsTransactionImpl::GetName() const {
  return name_;
}

uint16 MDnsTransactionImpl::GetType() const {
  return rrtype_;
}

void MDnsTransactionImpl::CacheRecordFound(const RecordParsed* record) {
  DCHECK(started_);
  OnRecordUpdate(MDnsListener::RECORD_ADDED, record);
}

void MDnsTransactionImpl::TriggerCallback(MDnsTransaction::Result result,
                                          const RecordParsed* record) {
  DCHECK(started_);
  if (!is_active()) return;

  // Ensure callback is run after touching all class state, so that
  // the callback can delete the transaction.
  MDnsTransaction::ResultCallback callback = callback_;

  // Reset the transaction if it expects a single result, or if the result
  // is a final one (everything except for a record).
  if (flags_ & MDnsTransaction::SINGLE_RESULT ||
      result != MDnsTransaction::RESULT_RECORD) {
    Reset();
  }

  callback.Run(result, record);
}

void MDnsTransactionImpl::Reset() {
  callback_.Reset();
  listener_.reset();
  timeout_.Cancel();
}

void MDnsTransactionImpl::OnRecordUpdate(MDnsListener::UpdateType update,
                                         const RecordParsed* record) {
  DCHECK(started_);
  if (update ==  MDnsListener::RECORD_ADDED ||
      update == MDnsListener::RECORD_CHANGED)
    TriggerCallback(MDnsTransaction::RESULT_RECORD, record);
}

void MDnsTransactionImpl::SignalTransactionOver() {
  DCHECK(started_);
  if (flags_ & MDnsTransaction::SINGLE_RESULT) {
    TriggerCallback(MDnsTransaction::RESULT_NO_RESULTS, NULL);
  } else {
    TriggerCallback(MDnsTransaction::RESULT_DONE, NULL);
  }
}

void MDnsTransactionImpl::ServeRecordsFromCache() {
  std::vector<const RecordParsed*> records;
  base::WeakPtr<MDnsTransactionImpl> weak_this = AsWeakPtr();

  if (client_->core()) {
    client_->core()->QueryCache(rrtype_, name_, &records);
    for (std::vector<const RecordParsed*>::iterator i = records.begin();
         i != records.end() && weak_this; ++i) {
      weak_this->TriggerCallback(MDnsTransaction::RESULT_RECORD, *i);
    }

#if defined(ENABLE_NSEC)
    if (records.empty()) {
      DCHECK(weak_this);
      client_->core()->QueryCache(dns_protocol::kTypeNSEC, name_, &records);
      if (!records.empty()) {
        const NsecRecordRdata* rdata =
            records.front()->rdata<NsecRecordRdata>();
        DCHECK(rdata);
        if (!rdata->GetBit(rrtype_))
          weak_this->TriggerCallback(MDnsTransaction::RESULT_NSEC, NULL);
      }
    }
#endif
  }
}

bool MDnsTransactionImpl::QueryAndListen() {
  listener_ = client_->CreateListener(rrtype_, name_, this);
  if (!listener_->Start())
    return false;

  DCHECK(client_->core());
  if (!client_->core()->SendQuery(rrtype_, name_))
    return false;

  timeout_.Reset(base::Bind(&MDnsTransactionImpl::SignalTransactionOver,
                            AsWeakPtr()));
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      timeout_.callback(),
      base::TimeDelta::FromSeconds(MDnsTransactionTimeoutSeconds));

  return true;
}

void MDnsTransactionImpl::OnNsecRecord(const std::string& name, unsigned type) {
  TriggerCallback(RESULT_NSEC, NULL);
}

void MDnsTransactionImpl::OnCachePurged() {
  // TODO(noamsml): Cache purge situations not yet implemented
}

}  // namespace net
