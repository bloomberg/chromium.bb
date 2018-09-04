// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/mdns_responder_adapter_impl.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>

#include "base/make_unique.h"
#include "platform/api/logging.h"

namespace openscreen {
namespace mdns {
namespace {

// RFC 1035 specifies a max string length of 256, including the leading length
// octet.
constexpr size_t kMaxDnsStringLength = 255;

void AssignMdnsPort(mDNSIPPort* mdns_port, uint16_t port) {
  mdns_port->b[0] = (port >> 8) & 0xff;
  mdns_port->b[1] = port & 0xff;
}

uint16_t GetNetworkOrderPort(const mDNSOpaque16& port) {
  return port.b[0] << 8 | port.b[1];
}

#if DCHECK_IS_ON()
bool IsValidServiceName(const std::string& service_name) {
  // Service name requirements come from RFC 6335:
  //  - No more than 16 characters.
  //  - Begin with '_'.
  //  - Next is a letter or digit and end with a letter or digit.
  //  - May contain hyphens, but no consecutive hyphens.
  //  - Must contain at least one letter.
  if (service_name.size() <= 1 || service_name.size() > 16) {
    return false;
  }
  if (service_name[0] != '_' || !std::isalnum(service_name[1]) ||
      !std::isalnum(service_name.back())) {
    return false;
  }
  bool has_alpha = false;
  bool previous_hyphen = false;
  for (auto it = service_name.begin() + 1; it != service_name.end(); ++it) {
    if (*it == '-' && previous_hyphen) {
      return false;
    }
    previous_hyphen = *it == '-';
    has_alpha = has_alpha || std::isalpha(*it);
  }
  return has_alpha && !previous_hyphen;
}

bool IsValidServiceProtocol(const std::string& protocol) {
  // RFC 6763 requires _tcp be used for TCP services and _udp for all others.
  return protocol == "_tcp" || protocol == "_udp";
}
#endif  // if DCHECK_IS_ON()

void MakeSubnetMaskFromPrefixLength(uint8_t mask[4], uint8_t prefix_length) {
  for (int i = 0; i < 4; prefix_length -= 8, ++i) {
    if (prefix_length >= 8) {
      mask[i] = 0xff;
    } else if (prefix_length > 0) {
      mask[i] = 0xff << (8 - prefix_length);
    } else {
      mask[i] = 0;
    }
  }
}

std::string MakeTxtData(const std::vector<std::string>& lines) {
  std::string txt;
  txt.reserve(256);
  for (const auto& line : lines) {
    const auto line_size = line.size();
    if (line_size > kMaxDnsStringLength ||
        (txt.size() + line_size + 1) > kMaxDnsStringLength) {
      return {};
    }
    txt.push_back(line_size);
    txt += line;
  }
  return txt;
}

MdnsResponderErrorCode MapMdnsError(int err) {
  switch (err) {
    case mStatus_NoError:
      return MdnsResponderErrorCode::kNoError;
    case mStatus_UnsupportedErr:
      return MdnsResponderErrorCode::kUnsupportedError;
    case mStatus_UnknownErr:
      return MdnsResponderErrorCode::kUnknownError;
    default:
      break;
  }
  DLOG_WARN << "unmapped mDNSResponder error: " << err;
  return MdnsResponderErrorCode::kUnknownError;
}

std::vector<std::string> ParseTxtResponse(const uint8_t data[256],
                                          uint16_t length) {
  DCHECK(length <= 256);
  if (length == 0) {
    return {};
  }
  std::vector<std::string> lines;
  int total_pos = 0;
  while (total_pos < length) {
    uint8_t line_length = data[total_pos];
    if ((line_length > kMaxDnsStringLength) ||
        (total_pos + line_length >= length)) {
      return {};
    }
    lines.emplace_back(&data[total_pos + 1],
                       &data[total_pos + line_length + 1]);
    total_pos += line_length + 1;
  }
  return lines;
}

void MdnsStatusCallback(mDNS* mdns, mStatus result) {
  LOG_INFO << "status good? " << (result == mStatus_NoError);
}

}  // namespace

MdnsResponderAdapterImpl::MdnsResponderAdapterImpl() = default;
MdnsResponderAdapterImpl::~MdnsResponderAdapterImpl() = default;

bool MdnsResponderAdapterImpl::Init() {
  const auto err =
      mDNS_Init(&mdns_, &platform_storage_, rr_cache_, kRrCacheSize,
                mDNS_Init_DontAdvertiseLocalAddresses, &MdnsStatusCallback,
                mDNS_Init_NoInitCallbackContext);
  return err == mStatus_NoError;
}

void MdnsResponderAdapterImpl::Close() {
  mDNS_StartExit(&mdns_);
  // Let all services send goodbyes.
  while (!service_records_.empty()) {
    RunTasks();
  }
  mDNS_FinalExit(&mdns_);
}

bool MdnsResponderAdapterImpl::SetHostLabel(const std::string& host_label) {
  if (host_label.size() > DomainName::kDomainNameMaxLabelLength) {
    return false;
  }
  MakeDomainLabelFromLiteralString(&mdns_.hostlabel, host_label.c_str());
  mDNS_SetFQDN(&mdns_);
  if (!service_records_.empty()) {
    DeadvertiseInterfaces();
    AdvertiseInterfaces();
  }
  return true;
}

bool MdnsResponderAdapterImpl::RegisterInterface(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv4Subnet& interface_address,
    platform::UdpSocketIPv4Ptr socket) {
  const auto info_it = v4_responder_interface_info_.find(socket);
  if (info_it != v4_responder_interface_info_.end()) {
    return true;
  }
  NetworkInterfaceInfo& info = v4_responder_interface_info_[socket];
  std::memset(&info, 0, sizeof(NetworkInterfaceInfo));
  info.InterfaceID = reinterpret_cast<decltype(info.InterfaceID)>(socket);
  info.Advertise = mDNSfalse;
  info.ip.type = mDNSAddrType_IPv4;
  interface_address.address.CopyTo(info.ip.ip.v4.b);
  info.mask.type = mDNSAddrType_IPv4;

  MakeSubnetMaskFromPrefixLength(info.mask.ip.v4.b,
                                 interface_address.prefix_length);
  interface_info.CopyHardwareAddressTo(info.MAC.b);
  info.McastTxRx = 1;
  platform_storage_.v4_sockets.push_back(socket);
  auto result = mDNS_RegisterInterface(&mdns_, &info, mDNSfalse);
  LOG_IF(WARN, result != mStatus_NoError)
      << "mDNS_RegisterInterface failed: " << result;
  return result == mStatus_NoError;
}

bool MdnsResponderAdapterImpl::RegisterInterface(
    const platform::InterfaceInfo& interface_info,
    const platform::IPv6Subnet& interface_address,
    platform::UdpSocketIPv6Ptr socket) {
  UNIMPLEMENTED();
  return false;
}

bool MdnsResponderAdapterImpl::DeregisterInterface(
    platform::UdpSocketIPv4Ptr socket) {
  const auto info_it = v4_responder_interface_info_.find(socket);
  if (info_it == v4_responder_interface_info_.end()) {
    return false;
  }
  const auto it = std::find(platform_storage_.v4_sockets.begin(),
                            platform_storage_.v4_sockets.end(), socket);
  DCHECK(it != platform_storage_.v4_sockets.end());
  platform_storage_.v4_sockets.erase(it);
  if (info_it->second.RR_A.namestorage.c[0]) {
    mDNS_Deregister(&mdns_, &info_it->second.RR_A);
    info_it->second.RR_A.namestorage.c[0] = 0;
  }
  mDNS_DeregisterInterface(&mdns_, &info_it->second, mDNSfalse);
  v4_responder_interface_info_.erase(info_it);
  return true;
}

bool MdnsResponderAdapterImpl::DeregisterInterface(
    platform::UdpSocketIPv6Ptr socket) {
  UNIMPLEMENTED();
  return false;
}

void MdnsResponderAdapterImpl::OnDataReceived(
    const IPv4Endpoint& source,
    const IPv4Endpoint& original_destination,
    const uint8_t* data,
    size_t length,
    platform::UdpSocketIPv4Ptr receiving_socket) {
  mDNSAddr src;
  src.type = mDNSAddrType_IPv4;
  source.address.CopyTo(src.ip.v4.b);
  mDNSIPPort srcport;
  AssignMdnsPort(&srcport, source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv4;
  original_destination.address.CopyTo(dst.ip.v4.b);
  mDNSIPPort dstport;
  AssignMdnsPort(&dstport, original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}
void MdnsResponderAdapterImpl::OnDataReceived(
    const IPv6Endpoint& source,
    const IPv6Endpoint& original_destination,
    const uint8_t* data,
    size_t length,
    platform::UdpSocketIPv6Ptr receiving_socket) {
  mDNSAddr src;
  src.type = mDNSAddrType_IPv6;
  source.address.CopyTo(src.ip.v6.b);
  mDNSIPPort srcport;
  AssignMdnsPort(&srcport, source.port);

  mDNSAddr dst;
  dst.type = mDNSAddrType_IPv6;
  original_destination.address.CopyTo(dst.ip.v6.b);
  mDNSIPPort dstport;
  AssignMdnsPort(&dstport, original_destination.port);

  mDNSCoreReceive(&mdns_, const_cast<uint8_t*>(data), data + length, &src,
                  srcport, &dst, dstport,
                  reinterpret_cast<mDNSInterfaceID>(receiving_socket));
}

int MdnsResponderAdapterImpl::RunTasks() {
  const auto t = mDNS_Execute(&mdns_);
  const auto now = mDNSPlatformRawTime();
  const auto next = t - now;
  return next;
}

std::vector<AEvent> MdnsResponderAdapterImpl::TakeAResponses() {
  return std::move(a_responses_);
}

std::vector<AaaaEvent> MdnsResponderAdapterImpl::TakeAaaaResponses() {
  return std::move(aaaa_responses_);
}

std::vector<PtrEvent> MdnsResponderAdapterImpl::TakePtrResponses() {
  return std::move(ptr_responses_);
}

std::vector<SrvEvent> MdnsResponderAdapterImpl::TakeSrvResponses() {
  return std::move(srv_responses_);
}

std::vector<TxtEvent> MdnsResponderAdapterImpl::TakeTxtResponses() {
  return std::move(txt_responses_);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StartAQuery(
    const DomainName& domain_name) {
  if (!domain_name.EndsWithLocalDomain()) {
    return MdnsResponderErrorCode::kInvalidParameters;
  }
  if (a_questions_.find(domain_name) != a_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  auto& question = a_questions_[domain_name];
  std::copy(domain_name.domain_name().begin(), domain_name.domain_name().end(),
            question.qname.c);

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  question.qtype = kDNSType_A;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::AQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StartAaaaQuery(
    const DomainName& domain_name) {
  if (!domain_name.EndsWithLocalDomain()) {
    return MdnsResponderErrorCode::kInvalidParameters;
  }
  if (aaaa_questions_.find(domain_name) != aaaa_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  auto& question = aaaa_questions_[domain_name];
  std::copy(domain_name.domain_name().begin(), domain_name.domain_name().end(),
            question.qname.c);

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  question.qtype = kDNSType_AAAA;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::AaaaQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StartPtrQuery(
    const DomainName& service_type) {
  if (ptr_questions_.find(service_type) != ptr_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }

  auto& question = ptr_questions_[service_type];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  if (service_type.EndsWithLocalDomain()) {
    std::copy(service_type.domain_name().begin(),
              service_type.domain_name().end(), question.qname.c);
  } else {
    DomainName service_type_with_local;
    if (!DomainName::Append(service_type, DomainName::kLocalDomain,
                            &service_type_with_local)) {
      return MdnsResponderErrorCode::kDomainOverflowError;
    }
    std::copy(service_type_with_local.domain_name().begin(),
              service_type_with_local.domain_name().end(), question.qname.c);
  }
  question.qtype = kDNSType_PTR;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::PtrQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StartSrvQuery(
    const DomainName& service_instance) {
  if (!service_instance.EndsWithLocalDomain()) {
    return MdnsResponderErrorCode::kInvalidParameters;
  }
  if (srv_questions_.find(service_instance) != srv_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  auto& question = srv_questions_[service_instance];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  std::copy(service_instance.domain_name().begin(),
            service_instance.domain_name().end(), question.qname.c);
  question.qtype = kDNSType_SRV;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::SrvQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StartTxtQuery(
    const DomainName& service_instance) {
  if (!service_instance.EndsWithLocalDomain()) {
    return MdnsResponderErrorCode::kInvalidParameters;
  }
  if (txt_questions_.find(service_instance) != txt_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  auto& question = txt_questions_[service_instance];

  question.InterfaceID = mDNSInterface_Any;
  question.Target = {0};
  std::copy(service_instance.domain_name().begin(),
            service_instance.domain_name().end(), question.qname.c);
  question.qtype = kDNSType_TXT;
  question.qclass = kDNSClass_IN;
  question.LongLived = mDNStrue;
  question.ExpectUnique = mDNSfalse;
  question.ForceMCast = mDNStrue;
  question.ReturnIntermed = mDNSfalse;
  question.SuppressUnusable = mDNSfalse;
  question.RetryWithSearchDomains = mDNSfalse;
  question.TimeoutQuestion = 0;
  question.WakeOnResolve = 0;
  question.SearchListIndex = 0;
  question.AppendSearchDomains = 0;
  question.AppendLocalSearchDomains = 0;
  question.qnameOrig = nullptr;
  question.QuestionCallback = &MdnsResponderAdapterImpl::TxtQueryCallback;
  question.QuestionContext = this;
  const auto err = mDNS_StartQuery(&mdns_, &question);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StartQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StopAQuery(
    const DomainName& domain_name) {
  auto entry = a_questions_.find(domain_name);
  if (entry == a_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  a_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StopAaaaQuery(
    const DomainName& domain_name) {
  auto entry = aaaa_questions_.find(domain_name);
  if (entry == aaaa_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  aaaa_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StopPtrQuery(
    const DomainName& service_type) {
  auto entry = ptr_questions_.find(service_type);
  if (entry == ptr_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  ptr_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StopSrvQuery(
    const DomainName& service_instance) {
  auto entry = srv_questions_.find(service_instance);
  if (entry == srv_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  srv_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::StopTxtQuery(
    const DomainName& service_instance) {
  auto entry = txt_questions_.find(service_instance);
  if (entry == txt_questions_.end()) {
    return MdnsResponderErrorCode::kNoError;
  }
  const auto err = mDNS_StopQuery(&mdns_, &entry->second);
  txt_questions_.erase(entry);
  LOG_IF(WARN, err != mStatus_NoError) << "mDNS_StopQuery failed: " << err;
  return MapMdnsError(err);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::RegisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol,
    const DomainName& target_host,
    uint16_t target_port,
    const std::vector<std::string>& lines) {
  DCHECK(IsValidServiceName(service_name));
  DCHECK(IsValidServiceProtocol(service_protocol));
  service_records_.push_back(MakeUnique<ServiceRecordSet>());
  auto* service_record = service_records_.back().get();
  domainlabel instance;
  domainlabel name;
  domainlabel protocol;
  domainname type;
  domainname domain;
  domainname host;
  mDNSIPPort port;

  MakeDomainLabelFromLiteralString(&instance, service_instance.c_str());
  MakeDomainLabelFromLiteralString(&name, service_name.c_str());
  MakeDomainLabelFromLiteralString(&protocol, service_protocol.c_str());
  type.c[0] = 0;
  AppendDomainLabel(&type, &name);
  AppendDomainLabel(&type, &protocol);
  std::copy(DomainName::kLocalDomain.domain_name().begin(),
            DomainName::kLocalDomain.domain_name().end(), domain.c);
  std::copy(target_host.domain_name().begin(), target_host.domain_name().end(),
            host.c);
  AssignMdnsPort(&port, target_port);
  auto txt = MakeTxtData(lines);
  if (txt.size() > 256) {
    // Not handling oversized TXT records.
    return MdnsResponderErrorCode::kUnsupportedError;
  }

  if (service_records_.size() == 1) {
    AdvertiseInterfaces();
  }

  auto result = mDNS_RegisterService(
      &mdns_, service_record, &instance, &type, &domain, &host, port,
      reinterpret_cast<const uint8_t*>(txt.data()), txt.size(), nullptr, 0,
      mDNSInterface_Any, &MdnsResponderAdapterImpl::ServiceCallback, this, 0);

  if (result != mStatus_NoError) {
    service_records_.pop_back();
    if (service_records_.empty()) {
      DeadvertiseInterfaces();
    }
  }
  return MapMdnsError(result);
}

MdnsResponderErrorCode MdnsResponderAdapterImpl::DeregisterService(
    const std::string& service_instance,
    const std::string& service_name,
    const std::string& service_protocol) {
  domainlabel instance;
  domainlabel name;
  domainlabel protocol;
  domainname type;
  domainname domain;
  domainname full_instance_name;

  MakeDomainLabelFromLiteralString(&instance, service_instance.c_str());
  MakeDomainLabelFromLiteralString(&name, service_name.c_str());
  MakeDomainLabelFromLiteralString(&protocol, service_protocol.c_str());
  type.c[0] = 0;
  AppendDomainLabel(&type, &name);
  AppendDomainLabel(&type, &protocol);
  std::copy(DomainName::kLocalDomain.domain_name().begin(),
            DomainName::kLocalDomain.domain_name().end(), domain.c);
  if (ConstructServiceName(&full_instance_name, &instance, &type, &domain)) {
    return MdnsResponderErrorCode::kInvalidParameters;
  }

  for (auto it = service_records_.begin(); it != service_records_.end(); ++it) {
    if (SameDomainName(&full_instance_name, &(*it)->RR_SRV.namestorage)) {
      // |it| will be removed from |service_records_| in ServiceCallback, when
      // mDNSResponder is done with the memory.
      mDNS_DeregisterService(&mdns_, it->get());
      return MdnsResponderErrorCode::kNoError;
    }
  }
  return MdnsResponderErrorCode::kNoError;
}

// static
void MdnsResponderAdapterImpl::AQueryCallback(mDNS* m,
                                              DNSQuestion* question,
                                              const ResourceRecord* answer,
                                              QC_result added) {
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_A);
  DomainName domain(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  IPv4Address address(answer->rdata->u.ipv4.b);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  auto event_type = QueryEventHeader::Type::kAddedNoCache;
  if (added == QC_add) {
    event_type = QueryEventHeader::Type::kAdded;
  } else if (added == QC_rmv) {
    event_type = QueryEventHeader::Type::kRemoved;
  } else {
    DCHECK_EQ(added, QC_addnocache);
  }
  adapter->a_responses_.emplace_back(
      QueryEventHeader{event_type, reinterpret_cast<platform::UdpSocketIPv4Ptr>(
                                       answer->InterfaceID)},
      std::move(domain), address);
}

// static
void MdnsResponderAdapterImpl::AaaaQueryCallback(mDNS* m,
                                                 DNSQuestion* question,
                                                 const ResourceRecord* answer,
                                                 QC_result added) {
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_A);
  DomainName domain(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  IPv6Address address(answer->rdata->u.ipv6.b);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  auto event_type = QueryEventHeader::Type::kAddedNoCache;
  if (added == QC_add) {
    event_type = QueryEventHeader::Type::kAdded;
  } else if (added == QC_rmv) {
    event_type = QueryEventHeader::Type::kRemoved;
  } else {
    DCHECK_EQ(added, QC_addnocache);
  }
  adapter->aaaa_responses_.emplace_back(
      QueryEventHeader{event_type, reinterpret_cast<platform::UdpSocketIPv4Ptr>(
                                       answer->InterfaceID)},
      std::move(domain), address);
}

// static
void MdnsResponderAdapterImpl::PtrQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_PTR);
  DomainName result(std::vector<uint8_t>(
      answer->rdata->u.name.c,
      answer->rdata->u.name.c + DomainNameLength(&answer->rdata->u.name)));

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  auto event_type = QueryEventHeader::Type::kAddedNoCache;
  if (added == QC_add) {
    event_type = QueryEventHeader::Type::kAdded;
  } else if (added == QC_rmv) {
    event_type = QueryEventHeader::Type::kRemoved;
  } else {
    DCHECK_EQ(added, QC_addnocache);
  }
  adapter->ptr_responses_.emplace_back(
      QueryEventHeader{event_type, reinterpret_cast<platform::UdpSocketIPv4Ptr>(
                                       answer->InterfaceID)},
      std::move(result));
}

// static
void MdnsResponderAdapterImpl::SrvQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_SRV);
  DomainName service(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  DomainName result(
      std::vector<uint8_t>(answer->rdata->u.srv.target.c,
                           answer->rdata->u.srv.target.c +
                               DomainNameLength(&answer->rdata->u.srv.target)));

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  auto event_type = QueryEventHeader::Type::kAddedNoCache;
  if (added == QC_add) {
    event_type = QueryEventHeader::Type::kAdded;
  } else if (added == QC_rmv) {
    event_type = QueryEventHeader::Type::kRemoved;
  } else {
    DCHECK_EQ(added, QC_addnocache);
  }
  adapter->srv_responses_.emplace_back(
      QueryEventHeader{event_type, reinterpret_cast<platform::UdpSocketIPv4Ptr>(
                                       answer->InterfaceID)},
      std::move(service), std::move(result),
      GetNetworkOrderPort(answer->rdata->u.srv.port));
}

// static
void MdnsResponderAdapterImpl::TxtQueryCallback(mDNS* m,
                                                DNSQuestion* question,
                                                const ResourceRecord* answer,
                                                QC_result added) {
  DCHECK(question);
  DCHECK(answer);
  DCHECK_EQ(answer->rrtype, kDNSType_TXT);
  DomainName service(std::vector<uint8_t>(
      question->qname.c,
      question->qname.c + DomainNameLength(&question->qname)));
  auto lines = ParseTxtResponse(answer->rdata->u.txt.c, answer->rdlength);

  auto* adapter =
      reinterpret_cast<MdnsResponderAdapterImpl*>(question->QuestionContext);
  DCHECK(adapter);
  auto event_type = QueryEventHeader::Type::kAddedNoCache;
  if (added == QC_add) {
    event_type = QueryEventHeader::Type::kAdded;
  } else if (added == QC_rmv) {
    event_type = QueryEventHeader::Type::kRemoved;
  } else {
    DCHECK_EQ(added, QC_addnocache);
  }
  adapter->txt_responses_.emplace_back(
      QueryEventHeader{event_type, reinterpret_cast<platform::UdpSocketIPv4Ptr>(
                                       answer->InterfaceID)},
      std::move(service), std::move(lines));
}

// static
void MdnsResponderAdapterImpl::ServiceCallback(mDNS* m,
                                               ServiceRecordSet* service_record,
                                               mStatus result) {
  if (result == mStatus_MemFree) {
    DLOG_INFO << "free service record";
    auto* adapter = reinterpret_cast<MdnsResponderAdapterImpl*>(
        service_record->ServiceContext);
    auto& service_records = adapter->service_records_;
    service_records.erase(
        std::remove_if(
            service_records.begin(), service_records.end(),
            [service_record](const std::unique_ptr<ServiceRecordSet>& sr) {
              return sr.get() == service_record;
            }),
        service_records.end());

    if (service_records.empty()) {
      adapter->DeadvertiseInterfaces();
    }
  }
}

void MdnsResponderAdapterImpl::AdvertiseInterfaces() {
  for (auto& info : v4_responder_interface_info_) {
    NetworkInterfaceInfo& interface_info = info.second;
    mDNS_SetupResourceRecord(&interface_info.RR_A, /** RDataStorage */ nullptr,
                             mDNSInterface_Any, kDNSType_A, kHostNameTTL,
                             kDNSRecordTypeUnique, AuthRecordAny,
                             /** Callback */ nullptr, /** Context */ nullptr);
    AssignDomainName(&interface_info.RR_A.namestorage,
                     &mdns_.MulticastHostname);
    interface_info.RR_A.resrec.rdata->u.ipv4 = interface_info.ip.ip.v4;
    mDNS_Register(&mdns_, &interface_info.RR_A);
  }
  for (auto& info : v6_responder_interface_info_) {
    NetworkInterfaceInfo& interface_info = info.second;
    mDNS_SetupResourceRecord(&interface_info.RR_A, /** RDataStorage */ nullptr,
                             mDNSInterface_Any, kDNSType_AAAA, kHostNameTTL,
                             kDNSRecordTypeUnique, AuthRecordAny,
                             /** Callback */ nullptr, /** Context */ nullptr);
    AssignDomainName(&interface_info.RR_A.namestorage,
                     &mdns_.MulticastHostname);
    interface_info.RR_A.resrec.rdata->u.ipv6 = interface_info.ip.ip.v6;
    mDNS_Register(&mdns_, &interface_info.RR_A);
  }
}

void MdnsResponderAdapterImpl::DeadvertiseInterfaces() {
  // Both loops below use the A resource record's domain name to determine
  // whether the record was advertised.  AdvertiseInterfaces sets the domain
  // name before registering the A record, and this clears it after
  // deregistering.
  for (auto& info : v4_responder_interface_info_) {
    NetworkInterfaceInfo& interface_info = info.second;
    if (interface_info.RR_A.namestorage.c[0]) {
      mDNS_Deregister(&mdns_, &interface_info.RR_A);
      interface_info.RR_A.namestorage.c[0] = 0;
    }
  }
  for (auto& info : v6_responder_interface_info_) {
    NetworkInterfaceInfo& interface_info = info.second;
    if (interface_info.RR_A.namestorage.c[0]) {
      mDNS_Deregister(&mdns_, &interface_info.RR_A);
      interface_info.RR_A.namestorage.c[0] = 0;
    }
  }
}

}  // namespace mdns
}  // namespace openscreen
