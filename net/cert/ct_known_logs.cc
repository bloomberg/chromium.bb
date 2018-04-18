// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_known_logs.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "crypto/sha2.h"

#if !defined(OS_NACL)
#include "net/cert/ct_log_verifier.h"
#endif

namespace net {

namespace ct {

namespace {

#include "net/data/ssl/certificate_transparency/log_list-inc.cc"

}  // namespace

#if !defined(OS_NACL)
std::vector<scoped_refptr<const CTLogVerifier>>
CreateLogVerifiersForKnownLogs() {
  std::vector<scoped_refptr<const CTLogVerifier>> verifiers;

  for (const auto& log : GetKnownLogs()) {
    scoped_refptr<const CTLogVerifier> log_verifier = CTLogVerifier::Create(
        base::StringPiece(log.log_key, log.log_key_length), log.log_name,
        log.log_dns_domain);
    // Make sure no null logs enter verifiers. Parsing of all statically
    // configured logs should always succeed, unless there has been binary or
    // memory corruption.
    CHECK(log_verifier);
    verifiers.push_back(std::move(log_verifier));
  }

  return verifiers;
}
#endif

std::vector<CTLogInfo> GetKnownLogs() {
  // Add all qualified logs.
  std::vector<CTLogInfo> logs(std::begin(kCTLogList), std::end(kCTLogList));

  // Add all disqualified logs. Callers are expected to filter verified SCTs
  // via IsLogDisqualified().
  for (const auto& disqualified_log : kDisqualifiedCTLogList) {
    logs.push_back(disqualified_log.log_info);
  }

  return logs;
}

bool IsLogOperatedByGoogle(base::StringPiece log_id) {
  CHECK_EQ(log_id.size(), crypto::kSHA256Length);

  return std::binary_search(std::begin(kGoogleLogIDs), std::end(kGoogleLogIDs),
                            log_id.data(), [](const char* a, const char* b) {
                              return memcmp(a, b, crypto::kSHA256Length) < 0;
                            });
}

bool IsLogDisqualified(base::StringPiece log_id,
                       base::Time* disqualification_date) {
  CHECK_EQ(log_id.size(), arraysize(kDisqualifiedCTLogList[0].log_id) - 1);

  auto* p = std::lower_bound(
      std::begin(kDisqualifiedCTLogList), std::end(kDisqualifiedCTLogList),
      log_id.data(),
      [](const DisqualifiedCTLogInfo& disqualified_log, const char* log_id) {
        return memcmp(disqualified_log.log_id, log_id, crypto::kSHA256Length) <
               0;
      });
  if (p == std::end(kDisqualifiedCTLogList) ||
      memcmp(p->log_id, log_id.data(), crypto::kSHA256Length) != 0) {
    return false;
  }

  *disqualification_date = base::Time::UnixEpoch() + p->disqualification_date;
  return true;
}

}  // namespace ct

}  // namespace net
