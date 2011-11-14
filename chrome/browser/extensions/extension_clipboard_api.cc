// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_clipboard_api.h"

#include <string>

#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "googleurl/src/gurl.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"

namespace {

enum SupportedMimeType {
  MIME_TYPE_TEXT,
  MIME_TYPE_HTML,
  // Not really a type. Used when iterating through enum values. Should allways
  // be the last value in the enum.
  MIME_TYPE_COUNT
};

const char kDataProperty[] = "data";
const char kUrlProperty[] = "url";

const char kMimeTypePlainText[] = "text/plain";
const char kMimeTypeHtml[] = "text/html";

const char kBufferStandard[] = "standard";
const char kBufferSelection[] = "selection";

const char kMimeTypeNotSupportedError[] = "MIME type is not supported.";
const char kBufferReadNotSupportedError[] =
    "Reading from the buffer is not supported.";
const char kBufferWriteNotSupportedError[] =
    "Writing to the buffer is not supported.";
const char kBufferNotSupportedError[] = "Buffer is not supported.";

// Converts data MIME type to data format known to ui::Clipboard. MIME type is
// given as a string. Returns false iff the string could not be converted.
bool ConvertMimeTypeStringToClipboardType(
    const std::string& mime_type,
    ui::Clipboard::FormatType* format_type) {
  if (mime_type == kMimeTypePlainText) {
    *format_type = ui::Clipboard::GetPlainTextFormatType();
    return true;
  }
  if (mime_type == kMimeTypeHtml) {
    *format_type = ui::Clipboard::GetHtmlFormatType();
    return true;
  }
  return false;
}

// Converts data MIME type to data format known to ui::Clipboard. MIME type is
// given as a enum value. Returns false iff the enum value could not be
// converted.
bool ConvertMimeTypeEnumToClipboardType(
    SupportedMimeType mime_type,
    ui::Clipboard::FormatType* format_type) {
  switch (mime_type) {
    case MIME_TYPE_TEXT:
      *format_type = ui::Clipboard::GetPlainTextFormatType();
      return true;
    case MIME_TYPE_HTML:
      *format_type = ui::Clipboard::GetHtmlFormatType();
      return true;
    default:
      return false;
  }
}

// Converts MIME type enum value to string. Returns false iff the enum could not
// be converted.
bool MimeTypeEnumToString(SupportedMimeType mime_type_enum,
                          std::string* mime_type_str) {
  switch (mime_type_enum) {
    case MIME_TYPE_TEXT:
      *mime_type_str = kMimeTypePlainText;
      return true;
    case MIME_TYPE_HTML:
      *mime_type_str = kMimeTypeHtml;
      return true;
    default:
      return false;
  }
}

// Converts a buffer type given as a string to buffer type known to
// ui::Clipboard. Returns false iff the conversion is not possible.
bool ConvertBufferTypeToClipboardType(const std::string& buffer,
                                      ui::Clipboard::Buffer* buffer_type) {
  if (buffer == kBufferStandard) {
    *buffer_type = ui::Clipboard::BUFFER_STANDARD;
    return true;
  }
  if (buffer == kBufferSelection) {
    *buffer_type = ui::Clipboard::BUFFER_SELECTION;
    return true;
  }
  return false;
}

}  // namespace

bool WriteDataClipboardFunction::RunImpl() {
  if (args_->GetSize() != 3 && args_->GetSize() != 4) {
    return false;
  }

  std::string mime_type;
  args_->GetString(0, &mime_type);

  std::string buffer;
  args_->GetString(1, &buffer);

  // At the moment only writing to standard cllipboard buffer is possible.
  if (buffer != kBufferStandard) {
    error_ = kBufferWriteNotSupportedError;
    return false;
  }

  if (mime_type == kMimeTypePlainText)
    return WritePlainText(buffer);

  if (mime_type == kMimeTypeHtml)
    return WriteHtml(buffer);

  error_ = kMimeTypeNotSupportedError;
  return false;
}

bool WriteDataClipboardFunction::WritePlainText(const std::string& buffer) {
  string16 data;
  args_->GetString(2, &data);

  ui::ScopedClipboardWriter scw(g_browser_process->clipboard());

  scw.WriteText(data);
  return true;
}

bool WriteDataClipboardFunction::WriteHtml(const std::string& buffer) {
  string16 data;
  args_->GetString(2, &data);

  std::string url_str;
  // |url| parameter is optional.
  if (args_->GetSize() == 4)
    args_->GetString(3, &url_str);

  // We do this to verify passed url is well-formed.
  GURL url(url_str);

  ui::ScopedClipboardWriter scw(g_browser_process->clipboard());

  scw.WriteHTML(data, url.spec());
  return true;
}

bool ReadDataClipboardFunction::RunImpl() {
  if (args_->GetSize() != 2) {
    return false;
  }
  std::string mime_type;
  args_->GetString(0, &mime_type);

  std::string buffer;
  args_->GetString(1, &buffer);

// Windows, Mac and Aura don't support selection clipboard buffer.
#if (defined(OS_WIN) || defined(OS_MACOSX) || defined(USE_AURA))
  if (buffer != kBufferStandard) {
    error_ = kBufferReadNotSupportedError;
    return false;
  }
#endif

  if (mime_type == kMimeTypePlainText)
    return ReadPlainText(buffer);

  if (mime_type == kMimeTypeHtml)
    return ReadHtml(buffer);

  error_ = kMimeTypeNotSupportedError;
  return false;
}

bool ReadDataClipboardFunction::ReadPlainText(const std::string& buffer) {
  string16 data;
  ui::Clipboard* clipboard = g_browser_process->clipboard();

  ui::Clipboard::Buffer clipboard_buffer;
  if (ConvertBufferTypeToClipboardType(buffer, &clipboard_buffer)) {
    clipboard->ReadText(clipboard_buffer, &data);
  } else {
    error_ = kBufferNotSupportedError;
    return false;
  }

  DictionaryValue* callbackData = new DictionaryValue();
  callbackData->SetString(kDataProperty, data);
  result_.reset(callbackData);
  return true;
}

bool ReadDataClipboardFunction::ReadHtml(const std::string& buffer) {
  ui::Clipboard* clipboard = g_browser_process->clipboard();

  string16 data;
  std::string url;
  uint32 fragment_start = 0;
  uint32 fragment_end = 0;
  ui::Clipboard::Buffer clipboard_buffer;

  if (ConvertBufferTypeToClipboardType(buffer, &clipboard_buffer)) {
    clipboard->ReadHTML(clipboard_buffer, &data, &url, &fragment_start,
        &fragment_end);
  } else {
    error_ = kBufferNotSupportedError;
    return false;
  }

  DictionaryValue* html_data = new base::DictionaryValue();
  size_t fragment_len = static_cast<size_t>(fragment_end - fragment_start);
  html_data->SetString(kDataProperty,
                       data.substr(static_cast<size_t>(fragment_start),
                                   fragment_len));
  html_data->SetString(kUrlProperty, url);
  result_.reset(html_data);

  return true;
}

bool GetAvailableMimeTypesClipboardFunction::RunImpl() {
  if (args_->GetSize() != 1) {
    return false;
  }

  std::string buffer_arg;
  args_->GetString(0, &buffer_arg);

  ui::Clipboard::Buffer buffer;
  if (!ConvertBufferTypeToClipboardType(buffer_arg, &buffer)) {
    error_ = kBufferNotSupportedError;
    return false;
  }

  ui::Clipboard* clipboard = g_browser_process->clipboard();

  ListValue* availableMimeTypes = new ListValue();

  for (int i = 0; i < MIME_TYPE_COUNT; i++) {
    ui::Clipboard::FormatType format;
    SupportedMimeType mime_type = static_cast<SupportedMimeType>(i);
    if (ConvertMimeTypeEnumToClipboardType(mime_type, &format) &&
        clipboard->IsFormatAvailable(format, buffer)) {
      std::string mime_type_str;
      if (!MimeTypeEnumToString(mime_type, &mime_type_str))
        continue;
      availableMimeTypes->Append(new StringValue(mime_type_str));
    }
  }

  result_.reset(availableMimeTypes);
  return true;
}
