// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/os_exchange_data_provider_gtk.h"

#include <algorithm>

#include "base/string_util.h"

OSExchangeDataProviderGtk::OSExchangeDataProviderGtk(
    int known_formats,
    const std::set<GdkAtom>& known_custom_formats)
    : known_formats_(known_formats),
      known_custom_formats_(known_custom_formats),
      formats_(0) {
}

OSExchangeDataProviderGtk::OSExchangeDataProviderGtk()
    : known_formats_(0),
      formats_(0) {
}

OSExchangeDataProviderGtk::~OSExchangeDataProviderGtk() {
}

bool OSExchangeDataProviderGtk::HasDataForAllFormats(
    int formats,
    const std::set<GdkAtom>& custom_formats) const {
  if ((formats_ & formats) != formats)
    return false;
  return std::includes(custom_formats_.begin(),
                       custom_formats_.end(),
                       custom_formats.begin(), custom_formats.end());
}

void OSExchangeDataProviderGtk::SetString(const std::wstring& data) {
  string_ = WideToUTF16Hack(data);
  formats_ |= OSExchangeData::STRING;
}

void OSExchangeDataProviderGtk::SetURL(const GURL& url,
                                       const std::wstring& title) {
  url_ = url;
  title_ = WideToUTF16Hack(title);
  formats_ |= OSExchangeData::URL;
}

void OSExchangeDataProviderGtk::SetFilename(const std::wstring& full_path) {
  filename_ = WideToUTF16Hack(full_path);
  formats_ |= OSExchangeData::FILE_NAME;
}

void OSExchangeDataProviderGtk::SetPickledData(GdkAtom format,
                                               const Pickle& data) {
  pickle_data_[format] = data;
  formats_ |= OSExchangeData::PICKLED_DATA;
}

void OSExchangeDataProviderGtk::SetFileContents(
    const std::wstring& filename,
    const std::string& file_contents) {
  filename_ = WideToUTF16Hack(filename);
  file_contents_ = file_contents;
  formats_ |= OSExchangeData::FILE_CONTENTS;
}

void OSExchangeDataProviderGtk::SetHtml(const std::wstring& html,
                                        const GURL& base_url) {
  html_ = WideToUTF16Hack(html);
  base_url_ = base_url;
  formats_ |= OSExchangeData::HTML;
}

bool OSExchangeDataProviderGtk::GetString(std::wstring* data) const {
  if (formats_ & OSExchangeData::STRING == 0)
    return false;
  *data = UTF16ToWideHack(string_);
  return true;
}

bool OSExchangeDataProviderGtk::GetURLAndTitle(GURL* url,
                                               std::wstring* title) const {
  if (formats_ & OSExchangeData::URL == 0)
    return false;
  if (!url_.is_valid())
    return false;

  *url = url_;
  *title = UTF16ToWideHack(title_);
  return true;
}

bool OSExchangeDataProviderGtk::GetFilename(std::wstring* full_path) const {
  if (formats_ & OSExchangeData::FILE_NAME == 0)
    return false;
  *full_path = UTF16ToWideHack(filename_);
  return true;
}

bool OSExchangeDataProviderGtk::GetPickledData(GdkAtom format,
                                               Pickle* data) const {
  PickleData::const_iterator i = pickle_data_.find(format);
  if (i == pickle_data_.end())
    return false;

  *data = i->second;
  return true;
}

bool OSExchangeDataProviderGtk::GetFileContents(
    std::wstring* filename,
    std::string* file_contents) const {
  if (formats_ & OSExchangeData::FILE_CONTENTS == 0)
    return false;
  *filename = UTF16ToWideHack(filename_);
  *file_contents = file_contents_;
  return true;
}

bool OSExchangeDataProviderGtk::GetHtml(std::wstring* html,
                                        GURL* base_url) const {
  if (formats_ & OSExchangeData::HTML == 0)
    return false;
  *html = UTF16ToWideHack(filename_);
  *base_url = base_url_;
  return true;
}

bool OSExchangeDataProviderGtk::HasString() const {
  return (known_formats_ & OSExchangeData::STRING) != 0 ||
         (formats_ & OSExchangeData::STRING) != 0;
}

bool OSExchangeDataProviderGtk::HasURL() const {
  return (known_formats_ & OSExchangeData::URL) != 0 ||
         (formats_ & OSExchangeData::URL) != 0;
}

bool OSExchangeDataProviderGtk::HasFile() const {
  return (known_formats_ & OSExchangeData::FILE_NAME) != 0 ||
         (formats_ & OSExchangeData::FILE_NAME) != 0;
  }

bool OSExchangeDataProviderGtk::HasFileContents() const {
  return (known_formats_ & OSExchangeData::FILE_CONTENTS) != 0 ||
         (formats_ & OSExchangeData::FILE_CONTENTS) != 0;
}

bool OSExchangeDataProviderGtk::HasHtml() const {
  return (known_formats_ & OSExchangeData::HTML) != 0 ||
         (formats_ & OSExchangeData::HTML) != 0;
}

bool OSExchangeDataProviderGtk::HasCustomFormat(GdkAtom format) const {
  return known_custom_formats_.find(format) != known_custom_formats_.end() ||
         custom_formats_.find(format) != custom_formats_.end();
}

///////////////////////////////////////////////////////////////////////////////
// OSExchangeData, public:

// static
OSExchangeData::Provider* OSExchangeData::CreateProvider() {
  return new OSExchangeDataProviderGtk();
}

GdkAtom OSExchangeData::RegisterCustomFormat(const std::string& type) {
  return gdk_atom_intern(type.c_str(), false);
}
