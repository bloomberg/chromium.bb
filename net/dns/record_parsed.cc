// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_parsed.h"

#include "net/dns/dns_response.h"
#include "net/dns/record_rdata.h"

namespace net {

RecordParsed::RecordParsed(const std::string& name, uint16 type, uint16 klass,
                           uint32 ttl, scoped_ptr<const RecordRdata> rdata)
    : name_(name), type_(type), klass_(klass), ttl_(ttl), rdata_(rdata.Pass()) {
}

RecordParsed::~RecordParsed() {
}

// static
scoped_ptr<const RecordParsed> RecordParsed::CreateFrom(
    DnsRecordParser* parser) {
  DnsResourceRecord record;
  scoped_ptr<const RecordRdata> rdata;

  if (!parser->ReadRecord(&record))
    return scoped_ptr<const RecordParsed>();

  switch (record.type) {
    case ARecordRdata::kType:
      rdata = ARecordRdata::Create(record.rdata, *parser);
      break;
    case CnameRecordRdata::kType:
      rdata = CnameRecordRdata::Create(record.rdata, *parser);
      break;
    case PtrRecordRdata::kType:
      rdata = PtrRecordRdata::Create(record.rdata, *parser);
      break;
    case SrvRecordRdata::kType:
      rdata = SrvRecordRdata::Create(record.rdata, *parser);
      break;
    case TxtRecordRdata::kType:
      rdata = TxtRecordRdata::Create(record.rdata, *parser);
      break;
    default:
      return scoped_ptr<const RecordParsed>();
  }

  if (!rdata.get())
    return scoped_ptr<const RecordParsed>();

  return scoped_ptr<const RecordParsed>(new RecordParsed(record.name,
                                                         record.type,
                                                         record.klass,
                                                         record.ttl,
                                                         rdata.Pass()));
}
}
