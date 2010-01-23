// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/resource_bundle.h"

#include "app/gfx/font.h"
#include "app/l10n_util.h"
#include "base/data_pack.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_piece.h"

namespace {

base::DataPack* LoadResourcesDataPak(FilePath resources_pak_path) {
  base::DataPack* resources_pak = new base::DataPack;
  bool success = resources_pak->Load(resources_pak_path);
  if (!success) {
    delete resources_pak;
    resources_pak = NULL;
  }
  return resources_pak;
}

}  // namespace

ResourceBundle::~ResourceBundle() {
  FreeImages();
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  FreeGdkPixBufs();
#endif
  delete locale_resources_data_;
  locale_resources_data_ = NULL;
  delete resources_data_;
  resources_data_ = NULL;
}

// static
RefCountedStaticMemory* ResourceBundle::LoadResourceBytes(
    DataHandle module, int resource_id) {
  DCHECK(module);
  return module->GetStaticMemory(resource_id);
}

base::StringPiece ResourceBundle::GetRawDataResource(int resource_id) {
  DCHECK(resources_data_);
  base::StringPiece data;
  if (!resources_data_->GetStringPiece(resource_id, &data)) {
    if (!locale_resources_data_->GetStringPiece(resource_id, &data)) {
      return base::StringPiece();
    }
  }
  return data;
}

string16 ResourceBundle::GetLocalizedString(int message_id) {
  // If for some reason we were unable to load a resource pak, return an empty
  // string (better than crashing).
  if (!locale_resources_data_) {
    LOG(WARNING) << "locale resources are not loaded";
    return string16();
  }

  base::StringPiece data;
  if (!locale_resources_data_->GetStringPiece(message_id, &data)) {
    // Fall back on the main data pack (shouldn't be any strings here except in
    // unittests).
    data = GetRawDataResource(message_id);
    if (data.empty()) {
      NOTREACHED() << "unable to find resource: " << message_id;
      return string16();
    }
  }

  // Data pack encodes strings as UTF16.
  DCHECK_EQ(data.length() % 2, 0U);
  string16 msg(reinterpret_cast<const char16*>(data.data()),
               data.length() / 2);
  return msg;
}

std::string ResourceBundle::LoadResources(const std::wstring& pref_locale) {
  DCHECK(!resources_data_) << "chrome.pak already loaded";
  FilePath resources_file_path = GetResourcesFilePath();
  CHECK(!resources_file_path.empty()) << "chrome.pak not found";
  resources_data_ = LoadResourcesDataPak(resources_file_path);
  CHECK(resources_data_) << "failed to load chrome.pak";

  DCHECK(!locale_resources_data_) << "locale.pak already loaded";
  std::string app_locale = l10n_util::GetApplicationLocale(pref_locale);
  FilePath locale_file_path = GetLocaleFilePath(app_locale);
  if (locale_file_path.empty()) {
    // It's possible that there is no locale.pak.
    NOTREACHED();
    return std::string();
  }
  locale_resources_data_ = LoadResourcesDataPak(locale_file_path);
  CHECK(locale_resources_data_) << "failed to load locale.pak";
  return app_locale;
}
