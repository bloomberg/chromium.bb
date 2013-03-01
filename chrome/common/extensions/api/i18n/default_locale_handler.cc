// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/api/i18n/default_locale_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest.h"
#include "ui/base/l10n/l10n_util.h"

namespace keys = extension_manifest_keys;

namespace extensions {

// static
const std::string& LocaleInfo::GetDefaultLocale(const Extension* extension) {
  LocaleInfo* info = static_cast<LocaleInfo*>(
      extension->GetManifestData(keys::kDefaultLocale));
  return info ? info->default_locale : EmptyString();
}

DefaultLocaleHandler::DefaultLocaleHandler() {
}

DefaultLocaleHandler::~DefaultLocaleHandler() {
}

bool DefaultLocaleHandler::Parse(Extension* extension, string16* error) {
  scoped_ptr<LocaleInfo> info(new LocaleInfo);
  if (!extension->manifest()->GetString(keys::kDefaultLocale,
                                        &info->default_locale) ||
      !l10n_util::IsValidLocaleSyntax(info->default_locale)) {
    *error = ASCIIToUTF16(extension_manifest_errors::kInvalidDefaultLocale);
    return false;
  }
  extension->SetManifestData(keys::kDefaultLocale, info.release());
  return true;
}

const std::vector<std::string> DefaultLocaleHandler::Keys() const {
  return SingleKey(keys::kDefaultLocale);
}

}  // namespace extensions
