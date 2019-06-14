// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_reader.h"

#include "cast/common/mdns/mdns_constants.h"
#include "platform/api/logging.h"

namespace cast {
namespace mdns {

MdnsReader::MdnsReader(const uint8_t* buffer, size_t length)
    : BigEndianReader(buffer, length) {}

bool MdnsReader::ReadCharacterString(std::string* out) {
  Cursor cursor(this);
  uint8_t string_length;
  if (Read<uint8_t>(&string_length)) {
    const char* string_begin = reinterpret_cast<const char*>(current());
    if (Skip(string_length)) {
      *out = std::string(string_begin, string_length);
      cursor.Commit();
      return true;
    }
  }
  return false;
}

// RFC 1035: https://www.ietf.org/rfc/rfc1035.txt
// See section 4.1.4. Message compression
bool MdnsReader::ReadDomainName(DomainName* out) {
  OSP_DCHECK(out);
  out->Clear();
  const uint8_t* position = current();
  // The number of bytes consumed reading from the starting position to either
  // the first label pointer or the final termination byte, including the
  // pointer or the termination byte. This is equal to the actual wire size of
  // the DomainName accounting for compression.
  size_t bytes_consumed = 0;
  // The number of bytes that was processed when reading the DomainName,
  // including all label pointers and direct labels. It is used to detect
  // circular compression. The number of processed bytes cannot be possibly
  // greater than the length of the buffer.
  size_t bytes_processed = 0;

  // If we are pointing before the beginning or past the end of the buffer, we
  // hit a malformed pointer. If we have processed more bytes than there are in
  // the buffer, we are in a circular compression loop.
  while (position >= begin() && position < end() &&
         bytes_processed <= length()) {
    const uint8_t label_type = openscreen::ReadBigEndian<uint8_t>(position);
    if (IsTerminationLabel(label_type)) {
      if (!bytes_consumed) {
        bytes_consumed = position + sizeof(uint8_t) - current();
      }
      return Skip(bytes_consumed);
    } else if (IsPointerLabel(label_type)) {
      if (position + sizeof(uint16_t) > end()) {
        return false;
      }
      const uint16_t label_offset =
          GetPointerLabelOffset(openscreen::ReadBigEndian<uint16_t>(position));
      if (!bytes_consumed) {
        bytes_consumed = position + sizeof(uint16_t) - current();
      }
      bytes_processed += sizeof(uint16_t);
      position = begin() + label_offset;
    } else if (IsDirectLabel(label_type)) {
      const uint8_t label_length = GetDirectLabelLength(label_type);
      OSP_DCHECK_GT(label_length, 0);
      bytes_processed += sizeof(uint8_t);
      position += sizeof(uint8_t);
      if (position + label_length >= end()) {
        return false;
      }
      if (!out->PushLabel(absl::string_view(
              reinterpret_cast<const char*>(position), label_length))) {
        return false;
      }
      bytes_processed += label_length;
      position += label_length;
    } else {
      return false;
    }
  }
  return false;
}

bool MdnsReader::ReadRawRecordRdata(RawRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    std::vector<uint8_t> buffer(record_length);
    if (ReadBytes(buffer.size(), buffer.data())) {
      *out = RawRecordRdata(std::move(buffer));
      cursor.Commit();
      return true;
    }
  }
  return false;
}

bool MdnsReader::ReadSrvRecordRdata(SrvRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  uint16_t priority;
  uint16_t weight;
  uint16_t port;
  DomainName target;
  if (Read<uint16_t>(&record_length) && Read<uint16_t>(&priority) &&
      Read<uint16_t>(&weight) && Read<uint16_t>(&port) &&
      ReadDomainName(&target) &&
      (cursor.delta() == sizeof(record_length) + record_length)) {
    *out = SrvRecordRdata(priority, weight, port, std::move(target));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::ReadARecordRdata(ARecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  IPAddress address;
  if (Read<uint16_t>(&record_length) && (record_length == IPAddress::kV4Size) &&
      ReadIPAddress(IPAddress::Version::kV4, &address)) {
    *out = ARecordRdata(address);
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::ReadAAAARecordRdata(AAAARecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  IPAddress address;
  if (Read<uint16_t>(&record_length) && (record_length == IPAddress::kV6Size) &&
      ReadIPAddress(IPAddress::Version::kV6, &address)) {
    *out = AAAARecordRdata(address);
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::ReadPtrRecordRdata(PtrRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  DomainName ptr_domain;
  if (Read<uint16_t>(&record_length) && ReadDomainName(&ptr_domain) &&
      (cursor.delta() == sizeof(record_length) + record_length)) {
    *out = PtrRecordRdata(std::move(ptr_domain));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::ReadTxtRecordRdata(TxtRecordRdata* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  uint16_t record_length;
  if (Read<uint16_t>(&record_length)) {
    std::vector<std::string> texts;
    while (cursor.delta() < sizeof(record_length) + record_length) {
      std::string entry;
      if (!ReadCharacterString(&entry)) {
        return false;
      }
      OSP_DCHECK(entry.length() <= kTXTMaxEntrySize);
      if (!entry.empty()) {
        texts.push_back(std::move(entry));
      }
    }
    if (cursor.delta() == sizeof(record_length) + record_length) {
      *out = TxtRecordRdata(std::move(texts));
      cursor.Commit();
      return true;
    }
  }
  return false;
}

bool MdnsReader::ReadMdnsRecord(MdnsRecord* out) {
  OSP_DCHECK(out);
  Cursor cursor(this);
  DomainName name;
  uint16_t type;
  uint16_t record_class;
  uint32_t ttl;
  Rdata rdata;
  if (ReadDomainName(&name) && Read<uint16_t>(&type) &&
      Read<uint16_t>(&record_class) && Read<uint32_t>(&ttl) &&
      ReadRdata(type, &rdata)) {
    *out =
        MdnsRecord(std::move(name), type, record_class, ttl, std::move(rdata));
    cursor.Commit();
    return true;
  }
  return false;
}

bool MdnsReader::ReadIPAddress(IPAddress::Version version, IPAddress* out) {
  size_t ipaddress_size = (version == IPAddress::Version::kV6)
                              ? IPAddress::kV6Size
                              : IPAddress::kV4Size;
  const uint8_t* const address_bytes = current();
  if (Skip(ipaddress_size)) {
    *out = IPAddress(version, address_bytes);
    return true;
  }
  return false;
}

bool MdnsReader::ReadRdata(uint16_t type, Rdata* out) {
  switch (type) {
    case kTypeSRV: {
      SrvRecordRdata srv_rdata;
      if (ReadSrvRecordRdata(&srv_rdata)) {
        *out = std::move(srv_rdata);
        return true;
      }
      return false;
    }
    case kTypeA: {
      ARecordRdata a_rdata;
      if (ReadARecordRdata(&a_rdata)) {
        *out = std::move(a_rdata);
        return true;
      }
      return false;
    }
    case kTypeAAAA: {
      AAAARecordRdata aaaa_rdata;
      if (ReadAAAARecordRdata(&aaaa_rdata)) {
        *out = std::move(aaaa_rdata);
        return true;
      }
      return false;
    }
    case kTypePTR: {
      PtrRecordRdata ptr_rdata;
      if (ReadPtrRecordRdata(&ptr_rdata)) {
        *out = std::move(ptr_rdata);
        return true;
      }
      return false;
    }
    case kTypeTXT: {
      TxtRecordRdata txt_rdata;
      if (ReadTxtRecordRdata(&txt_rdata)) {
        *out = std::move(txt_rdata);
        return true;
      }
      return false;
    }
    default: {
      RawRecordRdata raw_rdata;
      if (ReadRawRecordRdata(&raw_rdata)) {
        *out = std::move(raw_rdata);
        return true;
      }
      return false;
    }
  }
}

}  // namespace mdns
}  // namespace cast
