// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include <set>
#include <string>
#include <utility>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_l10n_util.h"
#include "extensions/common/message_bundle.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {
namespace file_util {
namespace {

// Returns true if the given file path exists and is not zero-length.
bool ValidateFilePath(const base::FilePath& path) {
  int64 size = 0;
  if (!base::PathExists(path) ||
      !base::GetFileSize(path, &size) ||
      size == 0) {
    return false;
  }

  return true;
}

}  // namespace

base::FilePath ExtensionURLToRelativeFilePath(const GURL& url) {
  std::string url_path = url.path();
  if (url_path.empty() || url_path[0] != '/')
    return base::FilePath();

  // Drop the leading slashes and convert %-encoded UTF8 to regular UTF8.
  std::string file_path = net::UnescapeURLComponent(url_path,
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  size_t skip = file_path.find_first_not_of("/\\");
  if (skip != file_path.npos)
    file_path = file_path.substr(skip);

  base::FilePath path = base::FilePath::FromUTF8Unsafe(file_path);

  // It's still possible for someone to construct an annoying URL whose path
  // would still wind up not being considered relative at this point.
  // For example: chrome-extension://id/c:////foo.html
  if (path.IsAbsolute())
    return base::FilePath();

  return path;
}

base::FilePath ExtensionResourceURLToFilePath(const GURL& url,
                                              const base::FilePath& root) {
  std::string host = net::UnescapeURLComponent(url.host(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  if (host.empty())
    return base::FilePath();

  base::FilePath relative_path = ExtensionURLToRelativeFilePath(url);
  if (relative_path.empty())
    return base::FilePath();

  base::FilePath path = root.AppendASCII(host).Append(relative_path);
  if (!base::PathExists(path))
    return base::FilePath();
  path = base::MakeAbsoluteFilePath(path);
  if (path.empty() || !root.IsParent(path))
    return base::FilePath();
  return path;
}

bool ValidateExtensionIconSet(const ExtensionIconSet& icon_set,
                              const Extension* extension,
                              int error_message_id,
                              std::string* error) {
  for (ExtensionIconSet::IconMap::const_iterator iter = icon_set.map().begin();
       iter != icon_set.map().end();
       ++iter) {
    const base::FilePath path =
        extension->GetResource(iter->second).GetFilePath();
    if (!ValidateFilePath(path)) {
      *error = l10n_util::GetStringFUTF8(error_message_id,
                                         base::UTF8ToUTF16(iter->second));
      return false;
    }
  }
  return true;
}

MessageBundle* LoadMessageBundle(
    const base::FilePath& extension_path,
    const std::string& default_locale,
    std::string* error) {
  error->clear();
  // Load locale information if available.
  base::FilePath locale_path = extension_path.Append(kLocaleFolder);
  if (!base::PathExists(locale_path))
    return NULL;

  std::set<std::string> locales;
  if (!extension_l10n_util::GetValidLocales(locale_path, &locales, error))
    return NULL;

  if (default_locale.empty() || locales.find(default_locale) == locales.end()) {
    *error = l10n_util::GetStringUTF8(
        IDS_EXTENSION_LOCALES_NO_DEFAULT_LOCALE_SPECIFIED);
    return NULL;
  }

  MessageBundle* message_bundle =
      extension_l10n_util::LoadMessageCatalogs(
          locale_path,
          default_locale,
          extension_l10n_util::CurrentLocaleOrDefault(),
          locales,
          error);

  return message_bundle;
}

std::map<std::string, std::string>* LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale) {
  std::map<std::string, std::string>* return_value =
      new std::map<std::string, std::string>();
  if (!default_locale.empty()) {
    // Touch disk only if extension is localized.
    std::string error;
    scoped_ptr<MessageBundle> bundle(
        LoadMessageBundle(extension_path, default_locale, &error));

    if (bundle.get())
      *return_value = *bundle->dictionary();
  }

  // Add @@extension_id reserved message here, so it's available to
  // non-localized extensions too.
  return_value->insert(
      std::make_pair(MessageBundle::kExtensionIdKey, extension_id));

  return return_value;
}

}  // namespace file_util
}  // namespace extensions
