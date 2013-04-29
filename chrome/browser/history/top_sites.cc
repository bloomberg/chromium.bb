// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"

#include "chrome/browser/history/top_sites_impl.h"

namespace history {

// static
TopSites* TopSites::Create(Profile* profile, const base::FilePath& db_name) {
  TopSitesImpl* top_sites_impl = new TopSitesImpl(profile);
  top_sites_impl->Init(db_name);
  return top_sites_impl;
}

}  // namespace history
