// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/common/translate_switches.h"

namespace translate {
namespace switches {

// Allows disabling of translate from the command line to assist with automated
// browser testing (e.g. Selenium/WebDriver). Normal browser users should
// disable translate with the preference.
const char kDisableTranslate[] = "disable-translate";

// Overrides the default server used for Google Translate.
const char kTranslateScriptURL[] = "translate-script-url";

// Overrides security-origin with which Translate runs in an isolated world.
const char kTranslateSecurityOrigin[] = "translate-security-origin";

}  // namespace switches
}  // namespace translate
