// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/gcd_url.h"

#include "base/command_line.h"
#include "chrome/browser/local_discovery/gcd_constants.h"
#include "chrome/common/chrome_switches.h"

namespace local_discovery {

GURL GetGcdURL() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  GURL server_url =
      GURL(command_line.GetSwitchValueASCII(switches::kGCDServiceURL));

  if (!server_url.is_valid()) {
    server_url = GURL(kGCDDefaultUrl);
  }

  return server_url;
}

}  // namespace local_discovery
