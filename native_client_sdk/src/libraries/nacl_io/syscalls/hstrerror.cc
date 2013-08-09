// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#ifdef PROVIDES_SOCKET_API

#include <sstream>
#include <string>
#include "sdk_util/macros.h"

EXTERN_C_BEGIN

const char* hstrerror(int err) {
  // These error message texts are taken straight from the man page
  const char* host_not_found_msg =
    "The specified host is unknown.";
  const char* no_address_msg =
    "The requested name is valid but does not have an IP address.";
  const char* no_recovery_msg =
    "A nonrecoverable name server error occurred.";
  const char* try_again_msg =
    "A temporary error occurred on an authoritative name server. "
    "Try again later.";
  const char* internal_msg =
    "Internal error in gethostbyname.";
  const char* unknown_msg_base =
    "Unknown error in gethostbyname: ";

  switch (err) {
    case HOST_NOT_FOUND:
      return host_not_found_msg;
    case NO_ADDRESS:
      return no_address_msg;
    case NO_RECOVERY:
      return no_recovery_msg;
    case TRY_AGAIN:
      return try_again_msg;
    case NETDB_INTERNAL:
      return internal_msg;
    default:
      std::stringstream msg;
      msg << unknown_msg_base << err << ".";

      static std::string unknown_msg;
      unknown_msg.assign(msg.str());
      return unknown_msg.c_str();
  }
}

EXTERN_C_END

#endif // PROVIDES_SOCKET_API
