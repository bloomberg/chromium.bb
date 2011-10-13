// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_
#define CHROME_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_
#pragma once

#include <string>

#include "net/base/net_errors.h"

enum InterruptReason {
  DOWNLOAD_INTERRUPT_REASON_NONE = 0,

#define INTERRUPT_REASON(name, value)  DOWNLOAD_INTERRUPT_REASON_##name = value,

#include "content/browser/download/interrupt_reason_values.h"

#undef INTERRUPT_REASON
};

enum DownloadInterruptSource {
  DOWNLOAD_INTERRUPT_FROM_DISK,
  DOWNLOAD_INTERRUPT_FROM_NETWORK,
  DOWNLOAD_INTERRUPT_FROM_SERVER,
  DOWNLOAD_INTERRUPT_FROM_USER,
  DOWNLOAD_INTERRUPT_FROM_CRASH
};

// Safe to call from any thread.
InterruptReason ConvertNetErrorToInterruptReason(
    net::Error file_error, DownloadInterruptSource source);

std::string InterruptReasonDebugString(InterruptReason error);

#endif  // CHROME_BROWSER_DOWNLOAD_INTERRUPT_REASONS_H_
