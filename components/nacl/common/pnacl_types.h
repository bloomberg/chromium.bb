// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_PNACL_TYPES_H_
#define COMPONENTS_NACL_COMMON_PNACL_TYPES_H_

// This file exists (instead of putting this type into nacl_types.h) because
// nacl_types is built into nacl_helper in addition to chrome, and we don't
// want to pull src/url/ into there, since it would be unnecessary bloat.

#include "base/basictypes.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace nacl {
// Cache-related information about pexe files, sent from the plugin/renderer
// to the browser.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_host_messages.h.
struct PnaclCacheInfo {
  PnaclCacheInfo();
  ~PnaclCacheInfo();
  GURL pexe_url;
  int abi_version;
  int opt_level;
  base::Time last_modified;
  std::string etag;
};

// Progress information for PNaCl on-demand installs.
struct PnaclInstallProgress {
  int64 current;
  int64 total_size;

  // Returns an instance of PnaclInstallProgress where the
  // total is marked as unknown.
  static PnaclInstallProgress Unknown();

  // Returns true if the given instance of PnaclInstallProgress has
  // an unknown total.
  static bool progress_known(const PnaclInstallProgress& p);
};

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_PNACL_TYPES_H_
