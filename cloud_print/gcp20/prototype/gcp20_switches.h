// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_GCP20_SWITCHES_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_GCP20_SWITCHES_H_

namespace switches {

extern const char kDisableConfirmation[];
extern const char kDisableIpv4[];
extern const char kDisableIpv6[];
extern const char kDisableMethodCheck[];
extern const char kDisableXTocken[];
extern const char kDomainName[];
extern const char kExtendedResponce[];
extern const char kHelpShort[];
extern const char kHelp[];
extern const char kHttpPort[];
extern const char kNoAnnouncement[];
extern const char kServiceName[];
extern const char kSimulatePrintingErrors[];
extern const char kStatePath[];
extern const char kTtl[];
extern const char kUnicastRespond[];

void PrintUsage();

}  // namespace switches

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_GCP20_SWITCHES_H_
