// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/chromium_api_delegate.h"

#include "chromeos/services/assistant/default_url_request_context_getter.h"

namespace chromeos {
namespace assistant {

ChromiumApiDelegate::ChromiumApiDelegate()
    : http_connection_factory_(
          base::MakeRefCounted<DefaultURLRequestContextGetter>(
              "chromium_http_connection")) {}

ChromiumApiDelegate::~ChromiumApiDelegate() = default;

assistant_client::HttpConnectionFactory*
ChromiumApiDelegate::GetHttpConnectionFactory() {
  return &http_connection_factory_;
}

}  // namespace assistant
}  // namespace chromeos
