// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_known_logs.h"

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "net/cert/ct_known_logs_static.h"
#include "net/cert/ct_log_verifier.h"

namespace net {

namespace ct {

ScopedVector<CTLogVerifier> CreateLogVerifiersForKnownLogs() {
  ScopedVector<CTLogVerifier> verifiers;
  for (size_t i = 0; i < arraysize(kCTLogList); ++i) {
    const CTLogInfo& log(kCTLogList[i]);
    base::StringPiece key(log.log_key, arraysize(log.log_key) - 1);

    verifiers.push_back(CTLogVerifier::Create(key, log.log_name).release());
  }

  return verifiers.Pass();
}

}  // namespace ct

}  // namespace net

