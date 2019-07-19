// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SharingMessageType {
  kUnknownMessage = 0,
  kPingMessage = 1,
  kAckMessage = 2,
  kClickToCallMessage = 3,
  kMaxValue = kClickToCallMessage,
};

SharingMessageType PayloadCaseToMessageType(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  switch (payload_case) {
    case chrome_browser_sharing::SharingMessage::PAYLOAD_NOT_SET:
      return SharingMessageType::kUnknownMessage;
    case chrome_browser_sharing::SharingMessage::kPingMessage:
      return SharingMessageType::kPingMessage;
    case chrome_browser_sharing::SharingMessage::kAckMessage:
      return SharingMessageType::kAckMessage;
    case chrome_browser_sharing::SharingMessage::kClickToCallMessage:
      return SharingMessageType::kClickToCallMessage;
  }
}

}  // namespace

void LogSharingMessageReceived(
    chrome_browser_sharing::SharingMessage::PayloadCase payload_case) {
  base::UmaHistogramEnumeration("Sharing.MessageReceivedType",
                                PayloadCaseToMessageType(payload_case));
}

void LogClickToCallDevicesToShow(int count) {
  base::UmaHistogramExactLinear("Sharing.ClickToCallDevicesToShow", count,
                                /*value_max=*/20);
}

void LogClickToCallAppsToShow(int count) {
  base::UmaHistogramExactLinear("Sharing.ClickToCallAppsToShow", count,
                                /*value_max=*/20);
}

void LogClickToCallSelectedDeviceIndex(const char* histogram_suffix,
                                       int index) {
  base::UmaHistogramExactLinear(
      base::StrCat(
          {"Sharing.ClickToCallSelectedDeviceIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}

void LogClickToCallSelectedAppIndex(const char* histogram_suffix, int index) {
  base::UmaHistogramExactLinear(
      base::StrCat({"Sharing.ClickToCallSelectedAppIndex.", histogram_suffix}),
      index,
      /*value_max=*/20);
}
