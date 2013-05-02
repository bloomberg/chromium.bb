// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/record_parsed.h"

#include "net/dns/dns_protocol.h"
#include "net/dns/dns_response.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/record_rdata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(RecordParsedTest, ParseSingleRecord) {
  DnsRecordParser parser(kT1ResponseDatagram, sizeof(kT1ResponseDatagram),
                         sizeof(dns_protocol::Header));
  scoped_ptr<const RecordParsed> record;
  const CnameRecordRdata* rdata;

  parser.SkipQuestion();
  record = RecordParsed::CreateFrom(&parser);
  EXPECT_TRUE(record != NULL);

  ASSERT_EQ("codereview.chromium.org", record->name());
  ASSERT_EQ(dns_protocol::kTypeCNAME, record->type());
  ASSERT_EQ(dns_protocol::kClassIN, record->klass());

  rdata = record->rdata<CnameRecordRdata>();
  ASSERT_TRUE(rdata != NULL);
  ASSERT_EQ(kT1CanonName, rdata->cname());

  ASSERT_FALSE(record->rdata<SrvRecordRdata>());
}
}
