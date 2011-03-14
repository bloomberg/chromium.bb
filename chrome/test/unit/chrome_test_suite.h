// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#define CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
#pragma once

#include <string>

#include "build/build_config.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/ref_counted.h"
#include "base/test/test_suite.h"
#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/common/chrome_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_util.h"

namespace base {
class StatsTable;
}

// In many cases it may be not obvious that a test makes a real DNS lookup.
// We generally don't want to rely on external DNS servers for our tests,
// so this host resolver procedure catches external queries and returns a failed
// lookup result.
class LocalHostResolverProc : public net::HostResolverProc {
 public:
  LocalHostResolverProc();
  virtual ~LocalHostResolverProc();

  virtual int Resolve(const std::string& host,
                      net::AddressFamily address_family,
                      net::HostResolverFlags host_resolver_flags,
                      net::AddressList* addrlist,
                      int* os_error);
};

class ChromeTestSuite : public base::TestSuite {
 public:
  ChromeTestSuite(int argc, char** argv);
  virtual ~ChromeTestSuite();

 protected:
  virtual void Initialize();
  virtual void Shutdown();

  void SetBrowserDirectory(const FilePath& browser_dir) {
    browser_dir_ = browser_dir;
  }

  // Client for embedding content in Chrome.
  chrome::ChromeContentClient chrome_content_client_;

  base::StatsTable* stats_table_;

  // The name used for the stats file so it can be cleaned up on posix during
  // test shutdown.
  std::string stats_filename_;

  // Alternative path to browser binaries.
  FilePath browser_dir_;

  ScopedOleInitializer ole_initializer_;
  scoped_refptr<LocalHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

#endif  // CHROME_TEST_UNIT_CHROME_TEST_SUITE_H_
