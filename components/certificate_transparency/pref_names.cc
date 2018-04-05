// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/pref_names.h"

namespace certificate_transparency {
namespace prefs {

const char kCTRequiredHosts[] = "certificate_transparency.required_hosts";

const char kCTExcludedHosts[] = "certificate_transparency.excluded_hosts";

const char kCTExcludedSPKIs[] = "certificate_transparency.excluded_spkis";

const char kCTExcludedLegacySPKIs[] =
    "certificate_transparency.excluded_legacy_spkis";

}  // namespace prefs
}  // namespace certificate_transparency
