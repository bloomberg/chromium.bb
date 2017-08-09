// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_utils.h"

#include "content/browser/download/download_interrupt_reasons_impl.h"
#include "content/browser/download/download_stats.h"

namespace content {

DownloadInterruptReason HandleRequestCompletionStatus(
    net::Error error_code, bool has_strong_validators,
    net::CertStatus cert_status, DownloadInterruptReason abort_reason) {
  // ERR_CONTENT_LENGTH_MISMATCH can be caused by 1 of the following reasons:
  // 1. Server or proxy closes the connection too early.
  // 2. The content-length header is wrong.
  // If the download has strong validators, we can interrupt the download
  // and let it resume automatically. Otherwise, resuming the download will
  // cause it to restart and the download may never complete if the error was
  // caused by reason 2. As a result, downloads without strong validators are
  // treated as completed here.
  // TODO(qinmin): check the metrics from downloads with strong validators,
  // and decide whether we should interrupt downloads without strong validators
  // rather than complete them.
  if (error_code == net::ERR_CONTENT_LENGTH_MISMATCH &&
      !has_strong_validators) {
    error_code = net::OK;
    RecordDownloadCount(COMPLETED_WITH_CONTENT_LENGTH_MISMATCH_COUNT);
  }

  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED == something outside of the network
    // stack cancelled the request.  There aren't that many things that
    // could do this to a download request (whose lifetime is separated from
    // the tab from which it came).  We map this to USER_CANCELLED as the
    // case we know about (system suspend because of laptop close) corresponds
    // to a user action.
    // TODO(asanka): A lid close or other power event should result in an
    // interruption that doesn't discard the partial state, unlike
     // USER_CANCELLED. (https://crbug.com/166179)
    if (net::IsCertStatusError(cert_status))
      return DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM;
    else
      return DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  } else if (abort_reason != DOWNLOAD_INTERRUPT_REASON_NONE) {
    // If a more specific interrupt reason was specified before the request
    // was explicitly cancelled, then use it.
    return abort_reason;
  }

  return ConvertNetErrorToInterruptReason(
      error_code, DOWNLOAD_INTERRUPT_FROM_NETWORK);
}

}  // namespace content
