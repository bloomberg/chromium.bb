// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_switches.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/strings/string_split.h"
#include "components/optimization_guide/proto/hints.pb.h"

namespace optimization_guide {
namespace switches {

// Overrides the Hints Protobuf that would come from the component updater. If
// the value of this switch is invalid, regular hint processing is used.
// The value of this switch should be a base64 encoding of a binary
// Configuration message, found in optimization_guide's hints.proto. Providing a
// valid value to this switch causes Chrome startup to block on hints parsing.
const char kHintsProtoOverride[] = "optimization_guide_hints_override";

// Overrides scheduling and time delays for fetching hints and causes a hints
// fetch immediately on start up using the provided comma separate lists of
// hosts.
const char kFetchHintsOverride[] = "optimization-guide-fetch-hints-override";

// Overrides the hints fetch scheduling and delay, causing a hints fetch
// immediately on start up using the TopHostProvider. This is meant for testing.
const char kFetchHintsOverrideTimer[] =
    "optimization-guide-fetch-hints-override-timer";

// Overrides the Optimization Guide Service URL that the HintsFetcher will
// request remote hints from.
const char kOptimizationGuideServiceURL[] = "optimization-guide-service-url";

// Overrides the Optimization Guide Service API Key for remote requests to be
// made.
const char kOptimizationGuideServiceAPIKey[] =
    "optimization-guide-service-api-key";

// Purges the hint cache store on startup, so that it's guaranteed to be using
// fresh data.
const char kPurgeHintCacheStore[] = "purge_hint_cache_store";

bool IsHintComponentProcessingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(kHintsProtoOverride);
}

bool ShouldPurgeHintCacheStoreOnStartup() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(kHintsProtoOverride) ||
         cmd_line->HasSwitch(kPurgeHintCacheStore);
}

// Parses a list of hosts to have hints fetched for. This overrides scheduling
// of the first hints fetch and forces it to occur immediately. If no hosts are
// provided, nullopt is returned.
base::Optional<std::vector<std::string>>
ParseHintsFetchOverrideFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kFetchHintsOverride))
    return base::nullopt;

  std::string override_hosts_value =
      cmd_line->GetSwitchValueASCII(kFetchHintsOverride);

  std::vector<std::string> hosts =
      base::SplitString(override_hosts_value, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);

  if (hosts.size() == 0)
    return base::nullopt;

  return hosts;
}

bool ShouldOverrideFetchHintsTimer() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kFetchHintsOverrideTimer);
}

std::unique_ptr<optimization_guide::proto::Configuration>
ParseComponentConfigFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kHintsProtoOverride))
    return nullptr;

  std::string b64_pb = cmd_line->GetSwitchValueASCII(kHintsProtoOverride);

  std::string binary_pb;
  if (!base::Base64Decode(b64_pb, &binary_pb)) {
    LOG(ERROR) << "Invalid base64 encoding of the Hints Proto Override";
    return nullptr;
  }

  std::unique_ptr<optimization_guide::proto::Configuration>
      proto_configuration =
          std::make_unique<optimization_guide::proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Invalid proto provided to the Hints Proto Override";
    return nullptr;
  }

  return proto_configuration;
}

}  // namespace switches
}  // namespace optimization_guide
