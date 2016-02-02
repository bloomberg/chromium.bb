// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/assignment_source.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "blimp/client/app/blimp_client_switches.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"

namespace blimp {
namespace {

// TODO(kmarshall): Take values from configuration data.
const char kDummyClientToken[] = "MyVoiceIsMyPassport";
const std::string kDefaultBlimpletIPAddress = "127.0.0.1";
const uint16_t kDefaultBlimpletTCPPort = 25467;

net::IPAddress GetBlimpletIPAddress() {
  std::string host;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBlimpletHost)) {
    host = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kBlimpletHost);
  } else {
    host = kDefaultBlimpletIPAddress;
  }
  net::IPAddress ip_address;
  if (!net::IPAddress::FromIPLiteral(host, &ip_address))
    CHECK(false) << "Invalid BlimpletAssignment host " << host;
  return ip_address;
}

uint16_t GetBlimpletTCPPort() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBlimpletTCPPort)) {
    std::string port_str =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kBlimpletTCPPort);
    uint port_64t;
    if (!base::StringToUint(port_str, &port_64t) ||
        !base::IsValueInRangeForNumericType<uint16_t>(port_64t)) {
      CHECK(false) << "Invalid BlimpletAssignment port " << port_str;
    }
    return base::checked_cast<uint16_t>(port_64t);
  } else {
    return kDefaultBlimpletTCPPort;
  }
}

}  // namespace

namespace client {

AssignmentSource::AssignmentSource(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : main_task_runner_(main_task_runner) {}

AssignmentSource::~AssignmentSource() {}

void AssignmentSource::GetAssignment(const AssignmentCallback& callback) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  Assignment assignment;
  assignment.ip_endpoint =
      net::IPEndPoint(GetBlimpletIPAddress(), GetBlimpletTCPPort());
  assignment.client_token = kDummyClientToken;
  main_task_runner_->PostTask(FROM_HERE, base::Bind(callback, assignment));
}

}  // namespace client
}  // namespace blimp
