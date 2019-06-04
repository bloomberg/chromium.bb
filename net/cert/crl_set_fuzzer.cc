// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/test/fuzzed_data_provider.h"
#include "net/cert/crl_set.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);
  const std::string str = data_provider.ConsumeRandomLengthString(size);
  scoped_refptr<net::CRLSet> out_crl_set;
  net::CRLSet::Parse(str, &out_crl_set);
  return 0;
}
