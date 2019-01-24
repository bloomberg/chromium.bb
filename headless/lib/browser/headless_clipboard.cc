// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_clipboard.h"

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace headless {

HeadlessClipboard::HeadlessClipboard()
    : default_store_type_(ui::CLIPBOARD_TYPE_COPY_PASTE) {}

HeadlessClipboard::~HeadlessClipboard() = default;

void HeadlessClipboard::OnPreShutdown() {}

uint64_t HeadlessClipboard::GetSequenceNumber(ui::ClipboardType type) const {
  return GetStore(type).sequence_number;
}

bool HeadlessClipboard::IsFormatAvailable(const ui::ClipboardFormatType& format,
                                          ui::ClipboardType type) const {
  const DataStore& store = GetStore(type);
  return store.data.find(format) != store.data.end();
}

void HeadlessClipboard::Clear(ui::ClipboardType type) {
  GetStore(type).Clear();
}

void HeadlessClipboard::ReadAvailableTypes(ui::ClipboardType type,
                                           std::vector<base::string16>* types,
                                           bool* contains_filenames) const {
  types->clear();

  if (IsFormatAvailable(ui::ClipboardFormatType::GetPlainTextType(), type))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeText));
  if (IsFormatAvailable(ui::ClipboardFormatType::GetHtmlType(), type))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeHTML));

  if (IsFormatAvailable(ui::ClipboardFormatType::GetRtfType(), type))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypeRTF));
  if (IsFormatAvailable(ui::ClipboardFormatType::GetBitmapType(), type))
    types->push_back(base::UTF8ToUTF16(ui::kMimeTypePNG));

  *contains_filenames = false;
}

void HeadlessClipboard::ReadText(ui::ClipboardType type,
                                 base::string16* result) const {
  std::string result8;
  ReadAsciiText(type, &result8);
  *result = base::UTF8ToUTF16(result8);
}

void HeadlessClipboard::ReadAsciiText(ui::ClipboardType type,
                                      std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(type);
  auto it = store.data.find(ui::ClipboardFormatType::GetPlainTextType());
  if (it != store.data.end())
    *result = it->second;
}

void HeadlessClipboard::ReadHTML(ui::ClipboardType type,
                                 base::string16* markup,
                                 std::string* src_url,
                                 uint32_t* fragment_start,
                                 uint32_t* fragment_end) const {
  markup->clear();
  src_url->clear();
  const DataStore& store = GetStore(type);
  auto it = store.data.find(ui::ClipboardFormatType::GetHtmlType());
  if (it != store.data.end())
    *markup = base::UTF8ToUTF16(it->second);
  *src_url = store.html_src_url;
  *fragment_start = 0;
  *fragment_end = base::checked_cast<uint32_t>(markup->size());
}

void HeadlessClipboard::ReadRTF(ui::ClipboardType type,
                                std::string* result) const {
  result->clear();
  const DataStore& store = GetStore(type);
  auto it = store.data.find(ui::ClipboardFormatType::GetRtfType());
  if (it != store.data.end())
    *result = it->second;
}

SkBitmap HeadlessClipboard::ReadImage(ui::ClipboardType type) const {
  return GetStore(type).image;
}

void HeadlessClipboard::ReadCustomData(ui::ClipboardType clipboard_type,
                                       const base::string16& type,
                                       base::string16* result) const {}

void HeadlessClipboard::ReadBookmark(base::string16* title,
                                     std::string* url) const {
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(ui::ClipboardFormatType::GetUrlWType());
  if (it != store.data.end())
    *url = it->second;
  *title = base::UTF8ToUTF16(store.url_title);
}

void HeadlessClipboard::ReadData(const ui::ClipboardFormatType& format,
                                 std::string* result) const {
  result->clear();
  const DataStore& store = GetDefaultStore();
  auto it = store.data.find(format);
  if (it != store.data.end())
    *result = it->second;
}

void HeadlessClipboard::WriteObjects(ui::ClipboardType type,
                                     const ObjectMap& objects) {
  Clear(type);
  default_store_type_ = type;
  for (const auto& kv : objects)
    DispatchObject(static_cast<ObjectType>(kv.first), kv.second);
  default_store_type_ = ui::CLIPBOARD_TYPE_COPY_PASTE;
}

void HeadlessClipboard::WriteText(const char* text_data, size_t text_len) {
  std::string text(text_data, text_len);
  GetDefaultStore().data[ui::ClipboardFormatType::GetPlainTextType()] = text;
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::GetPlainTextType()];
  if (IsSupportedClipboardType(ui::CLIPBOARD_TYPE_SELECTION)) {
    GetStore(ui::CLIPBOARD_TYPE_SELECTION)
        .data[ui::ClipboardFormatType::GetPlainTextType()] = text;
  }
}

void HeadlessClipboard::WriteHTML(const char* markup_data,
                                  size_t markup_len,
                                  const char* url_data,
                                  size_t url_len) {
  base::string16 markup;
  base::UTF8ToUTF16(markup_data, markup_len, &markup);
  GetDefaultStore().data[ui::ClipboardFormatType::GetHtmlType()] =
      base::UTF16ToUTF8(markup);
  GetDefaultStore().html_src_url = std::string(url_data, url_len);
}

void HeadlessClipboard::WriteRTF(const char* rtf_data, size_t data_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::GetRtfType()] =
      std::string(rtf_data, data_len);
}

void HeadlessClipboard::WriteBookmark(const char* title_data,
                                      size_t title_len,
                                      const char* url_data,
                                      size_t url_len) {
  GetDefaultStore().data[ui::ClipboardFormatType::GetUrlWType()] =
      std::string(url_data, url_len);
  GetDefaultStore().url_title = std::string(title_data, title_len);
}

void HeadlessClipboard::WriteWebSmartPaste() {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::GetWebKitSmartPasteType()];
}

void HeadlessClipboard::WriteBitmap(const SkBitmap& bitmap) {
  // Create a dummy entry.
  GetDefaultStore().data[ui::ClipboardFormatType::GetBitmapType()];
  SkBitmap& dst = GetDefaultStore().image;
  if (dst.tryAllocPixels(bitmap.info())) {
    bitmap.readPixels(dst.info(), dst.getPixels(), dst.rowBytes(), 0, 0);
  }
}

void HeadlessClipboard::WriteData(const ui::ClipboardFormatType& format,
                                  const char* data_data,
                                  size_t data_len) {
  GetDefaultStore().data[format] = std::string(data_data, data_len);
}

HeadlessClipboard::DataStore::DataStore() : sequence_number(0) {}

HeadlessClipboard::DataStore::DataStore(const DataStore& other) = default;

HeadlessClipboard::DataStore::~DataStore() = default;

void HeadlessClipboard::DataStore::Clear() {
  data.clear();
  url_title.clear();
  html_src_url.clear();
  image = SkBitmap();
}

const HeadlessClipboard::DataStore& HeadlessClipboard::GetStore(
    ui::ClipboardType type) const {
  CHECK(IsSupportedClipboardType(type));
  return stores_[type];
}

HeadlessClipboard::DataStore& HeadlessClipboard::GetStore(
    ui::ClipboardType type) {
  CHECK(IsSupportedClipboardType(type));
  DataStore& store = stores_[type];
  ++store.sequence_number;
  return store;
}

const HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() const {
  return GetStore(default_store_type_);
}

HeadlessClipboard::DataStore& HeadlessClipboard::GetDefaultStore() {
  return GetStore(default_store_type_);
}

}  // namespace headless
