// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_config/ssl_config_switches.h"

namespace switches {

// Specifies the maximum SSL/TLS version ("tls1", "tls1.1", "tls1.2", or
// "tls1.3").
const char kSSLVersionMax[] = "ssl-version-max";

// Specifies the minimum SSL/TLS version ("tls1", "tls1.1", "tls1.2", or
// "tls1.3").
const char kSSLVersionMin[] = "ssl-version-min";

// Specifies the enabled TLS 1.3 variant ("disabled", "draft", "experiment").
const char kTLS13Variant[] = "tls13-variant";

// These values aren't switches, but rather the values that kSSLVersionMax and
// kSSLVersionMin can have.
const char kSSLVersionTLSv1[] = "tls1";
const char kSSLVersionTLSv11[] = "tls1.1";
const char kSSLVersionTLSv12[] = "tls1.2";
const char kSSLVersionTLSv13[] = "tls1.3";

const char kTLS13VariantDisabled[] = "disabled";
const char kTLS13VariantDraft[] = "draft";
const char kTLS13VariantExperiment[] = "experiment";
const char kTLS13VariantExperiment2[] = "experiment2";
const char kTLS13VariantExperiment3[] = "experiment3";

}  // namespace switches
