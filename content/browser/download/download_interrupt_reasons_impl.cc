// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_interrupt_reasons_impl.h"

#include "base/logging.h"

namespace content {

#define FILE_ERROR_TO_INTERRUPT_REASON(n, d)  \
    case net::ERR_##n: return DOWNLOAD_INTERRUPT_REASON_FILE_##d;

#define NET_ERROR_TO_INTERRUPT_REASON(n, d)  \
    case net::ERR_##n: return DOWNLOAD_INTERRUPT_REASON_NETWORK_##d;

#define SERVER_ERROR_TO_INTERRUPT_REASON(n, d)  \
    case net::ERR_##n: return DOWNLOAD_INTERRUPT_REASON_SERVER_##d;

DownloadInterruptReason ConvertNetErrorToInterruptReason(
    net::Error net_error, DownloadInterruptSource source) {
  switch (net_error) {
    // File errors.
    case net::OK: return DOWNLOAD_INTERRUPT_REASON_NONE;

    // The file is too large.
    FILE_ERROR_TO_INTERRUPT_REASON(FILE_TOO_BIG, TOO_LARGE)

    // Permission to access a resource, other than the network, was denied.
    FILE_ERROR_TO_INTERRUPT_REASON(ACCESS_DENIED, ACCESS_DENIED)

    // There were not enough resources to complete the operation.
    FILE_ERROR_TO_INTERRUPT_REASON(INSUFFICIENT_RESOURCES, TRANSIENT_ERROR)

    // Memory allocation failed.
    FILE_ERROR_TO_INTERRUPT_REASON(OUT_OF_MEMORY, TRANSIENT_ERROR)

    // The path or file name is too long.
    FILE_ERROR_TO_INTERRUPT_REASON(FILE_PATH_TOO_LONG, NAME_TOO_LONG)

    // Not enough room left on the disk.
    FILE_ERROR_TO_INTERRUPT_REASON(FILE_NO_SPACE, NO_SPACE)

    // The file has a virus.
    FILE_ERROR_TO_INTERRUPT_REASON(FILE_VIRUS_INFECTED, VIRUS_INFECTED)

    // Network errors.

    // The network operation timed out.
    NET_ERROR_TO_INTERRUPT_REASON(TIMED_OUT, TIMEOUT)

    // The network connection has been lost.
    NET_ERROR_TO_INTERRUPT_REASON(INTERNET_DISCONNECTED, DISCONNECTED)

    // The server has gone down.
    NET_ERROR_TO_INTERRUPT_REASON(CONNECTION_FAILED, SERVER_DOWN)

    // The server has terminated the connection.
//    NET_ERROR_TO_INTERRUPT_REASON(CONNECTION_RESET, SERVER_DISCONNECTED)

    // The server has aborted the connection.
//    NET_ERROR_TO_INTERRUPT_REASON(CONNECTION_ABORTED, SERVER_ABORTED)

    // Server responses.

    // The server does not support range requests.
    SERVER_ERROR_TO_INTERRUPT_REASON(REQUEST_RANGE_NOT_SATISFIABLE, NO_RANGE)

    default: break;
  }

  // Handle errors that don't have mappings, depending on the source.
  switch (source) {
    case DOWNLOAD_INTERRUPT_FROM_DISK:
      return DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_NETWORK:
      return DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
    case DOWNLOAD_INTERRUPT_FROM_SERVER:
      return DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    default:
      break;
  }

  NOTREACHED();

  return DOWNLOAD_INTERRUPT_REASON_NONE;
}

#undef FILE_ERROR_TO_INTERRUPT_REASON
#undef NET_ERROR_TO_INTERRUPT_REASON
#undef SERVER_ERROR_TO_INTERRUPT_REASON

std::string InterruptReasonDebugString(DownloadInterruptReason error) {

#define INTERRUPT_REASON(name, value)  \
    case DOWNLOAD_INTERRUPT_REASON_##name: return #name;

  switch (error) {
    INTERRUPT_REASON(NONE, 0)

#include "content/public/browser/download_interrupt_reason_values.h"

    default:
      break;
  }

#undef INTERRUPT_REASON

  return "Unknown error";
}

}  // namespace content
