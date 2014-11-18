// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dns_prefetch/common/prefetch_messages.h"

#include "base/strings/string_number_conversions.h"
#include "components/dns_prefetch/common/prefetch_common.h"

namespace IPC {

void ParamTraits<dns_prefetch::LookupRequest>::Write(
    Message* m, const dns_prefetch::LookupRequest& request) {
  IPC::WriteParam(m, request.hostname_list);
}

bool ParamTraits<dns_prefetch::LookupRequest>::Read(
    const Message* m,
    PickleIterator* iter,
    dns_prefetch::LookupRequest* request) {
  // Verify the hostname limits after deserialization success.
  if (IPC::ReadParam(m, iter, &request->hostname_list)) {
    dns_prefetch::NameList& hostnames = request->hostname_list;
    if (hostnames.size() > dns_prefetch::kMaxDnsHostnamesPerRequest)
      return false;

    for (const auto& hostname : hostnames) {
      if (hostname.length() > dns_prefetch::kMaxDnsHostnameLength)
        return false;
    }
  }
  return true;
}

void ParamTraits<dns_prefetch::LookupRequest>::Log(
    const dns_prefetch::LookupRequest& p, std::string* l) {
  l->append("<dns_prefetch::LookupRequest: ");
  l->append(base::SizeTToString(p.hostname_list.size()));
  l->append(" hostnames>");
}

}  // namespace IPC
