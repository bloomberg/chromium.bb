// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate_net_log_param.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "net/cert/x509_certificate.h"
#include "net/log/net_log_capture_mode.h"

namespace net {

base::Value NetLogX509CertificateCallback(const X509Certificate* certificate,
                                          NetLogCaptureMode capture_mode) {
  base::DictionaryValue dict;
  base::ListValue certs;
  std::vector<std::string> encoded_chain;
  certificate->GetPEMEncodedChain(&encoded_chain);
  for (size_t i = 0; i < encoded_chain.size(); ++i)
    certs.AppendString(encoded_chain[i]);
  dict.SetKey("certificates", std::move(certs));
  return std::move(dict);
}

}  // namespace net
