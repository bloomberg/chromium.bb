// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/favicon/chrome_fallback_icon_client.h"

#include "build/build_config.h"
#include "chrome/grit/platform_locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

ChromeFallbackIconClient::ChromeFallbackIconClient() {
#if defined(OS_CHROMEOS)
  font_list_.push_back("Noto Sans");
#else
  font_list_.push_back(l10n_util::GetStringUTF8(IDS_SANS_SERIF_FONT_FAMILY));
#endif
}

ChromeFallbackIconClient::~ChromeFallbackIconClient() {
}

const std::vector<std::string>& ChromeFallbackIconClient::GetFontNameList()
    const {
  return font_list_;
}
