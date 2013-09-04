// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/common/pnacl_types.h"

namespace nacl {

PnaclCacheInfo::PnaclCacheInfo() {}
PnaclCacheInfo::~PnaclCacheInfo() {}

// static
bool PnaclInstallProgress::progress_known(const PnaclInstallProgress& p) {
  return p.total_size >= 0;
}

// static
PnaclInstallProgress PnaclInstallProgress::Unknown() {
  PnaclInstallProgress p;
  p.current = 0;
  // Use -1 to indicate that total is not determined.
  // This matches the -1 of the OnURLFetchDownloadProgress interface in
  // net/url_request/url_fetcher_delegate.h
  p.total_size = -1;
  return p;
}

}  // namespace nacl
