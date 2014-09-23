// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/gcp20_switches.h"

#include "base/files/file_path.h"
#include "base/path_service.h"

namespace switches {

const char kDisableConfirmation[] = "disable-confirmation";
const char kDisableIpv4[] = "disable-ipv4";
const char kDisableIpv6[] = "disable-ipv6";
const char kDisableMethodCheck[] = "disable-method-check";
const char kDisableXTocken[] = "disable-x-token";
const char kDomainName[] = "domain-name";
const char kExtendedResponce[] = "extended-response";
const char kHelpShort[] = "h";
const char kHelp[] = "help";
const char kHttpPort[] = "http-port";
const char kNoAnnouncement[] = "no-announcement";
const char kServiceName[] = "service-name";
const char kSimulatePrintingErrors[] = "simulate-printing-errors";
const char kStatePath[] = "state-path";
const char kTtl[] = "ttl";
const char kUnicastRespond[] = "unicast-respond";

const struct {
  const char* const name;
  const char* const description;
  const char* const arg;
} kHelpStrings[] = {
  {kDisableConfirmation, "disables confirmation of registration", NULL},
  {kDisableIpv4, "disables IPv4 support", NULL},
  {kDisableIpv6, "disables IPv6 support", NULL},
  {kDisableMethodCheck, "disables HTTP method checking (POST, GET)", NULL},
  {kDisableXTocken, "disables checking of X-Privet-Token HTTP header", NULL},
  {kNoAnnouncement, "disables DNS announcements", NULL},
  {kExtendedResponce, "responds to PTR with additional records", NULL},
  {kSimulatePrintingErrors, "simulates some errors for local printing", NULL},
  {kUnicastRespond,
       "DNS responses will be sent in unicast instead of multicast", NULL},
  {kDomainName, "sets, should ends with '.local'", "DOMAIN"},
  {kHttpPort, "sets port for HTTP server", "PORT"},
  {kServiceName, "sets DNS service name", "SERVICE"},
  {kStatePath, "sets path to file with registration state", "PATH"},
  {kTtl, "sets TTL for DNS announcements", "TTL"},
};

void PrintUsage() {
  base::FilePath exe;
  PathService::Get(base::FILE_EXE, &exe);
  printf("usage: %s [OPTION]...\n\n", exe.BaseName().MaybeAsASCII().c_str());
  for (size_t i = 0; i < arraysize(kHelpStrings); ++i) {
    std::string name = kHelpStrings[i].name;
    if (kHelpStrings[i].arg) {
      name += '=';
      name += kHelpStrings[i].arg;
    }
    name.resize(27, ' ');
    printf("  --%s%s\n", name.c_str(), kHelpStrings[i].description);
  }
  printf("\n  WARNING: mDNS probing is not implemented\n");
}

}  // namespace switches
