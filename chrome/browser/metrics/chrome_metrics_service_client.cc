// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_client.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/crash_keys.h"

namespace {

metrics::SystemProfileProto::Channel AsProtobufChannel(
    chrome::VersionInfo::Channel channel) {
  switch (channel) {
    case chrome::VersionInfo::CHANNEL_UNKNOWN:
      return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
    case chrome::VersionInfo::CHANNEL_CANARY:
      return metrics::SystemProfileProto::CHANNEL_CANARY;
    case chrome::VersionInfo::CHANNEL_DEV:
      return metrics::SystemProfileProto::CHANNEL_DEV;
    case chrome::VersionInfo::CHANNEL_BETA:
      return metrics::SystemProfileProto::CHANNEL_BETA;
    case chrome::VersionInfo::CHANNEL_STABLE:
      return metrics::SystemProfileProto::CHANNEL_STABLE;
  }
  NOTREACHED();
  return metrics::SystemProfileProto::CHANNEL_UNKNOWN;
}

}  // namespace

ChromeMetricsServiceClient::ChromeMetricsServiceClient() {
}

ChromeMetricsServiceClient::~ChromeMetricsServiceClient() {
}

void ChromeMetricsServiceClient::SetClientID(const std::string& client_id) {
  crash_keys::SetClientID(client_id);
}

bool ChromeMetricsServiceClient::IsOffTheRecordSessionActive() {
  return !chrome::IsOffTheRecordSessionActive();
}

std::string ChromeMetricsServiceClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

bool ChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return google_util::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel ChromeMetricsServiceClient::GetChannel() {
  return AsProtobufChannel(chrome::VersionInfo::GetChannel());
}

std::string ChromeMetricsServiceClient::GetVersionString() {
  // TODO(asvitkine): Move over from metrics_log.cc
  return std::string();
}
