// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/openssl_util.h"

#include <openssl/err.h>

#include "base/logging.h"

namespace base {

void ClearOpenSSLERRStack() {
  if (logging::DEBUG_MODE && VLOG_IS_ON(1)) {
    int error_num = ERR_get_error();
    if (error_num == 0)
      return;

    DVLOG(1) << "OpenSSL ERR_get_error stack:";
    char buf[140];
    do {
      ERR_error_string_n(error_num, buf, arraysize(buf));
      DVLOG(1) << "\t" << error_num << ": " << buf;
      error_num = ERR_get_error();
    } while (error_num != 0);
  } else {
    ERR_clear_error();
  }
}

}  // namespace base
