// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_STICKY_SETTINGS_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_STICKY_SETTINGS_H_

#include "printing/print_job_constants.h"

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"

class FilePath;
class PrintPreviewHandlerTest;

namespace base {
class DictionaryValue;
}

namespace printing {
struct PageSizeMargins;
}

FORWARD_DECLARE_TEST(PrintPreviewHandlerTest, StickyMarginsCustom);
FORWARD_DECLARE_TEST(PrintPreviewHandlerTest, StickyMarginsDefault);
FORWARD_DECLARE_TEST(PrintPreviewHandlerTest, StickyMarginsCustomThenDefault);
FORWARD_DECLARE_TEST(PrintPreviewHandlerTest, GetLastUsedMarginSettingsCustom);
FORWARD_DECLARE_TEST(PrintPreviewHandlerTest, GetLastUsedMarginSettingsDefault);

namespace printing {

// Holds all the settings that should be remembered (sticky) in print preview.
// A sticky setting will be restored next time the user launches print preview.
// Only one instance of this class is instantiated.
class StickySettings {
 public:
  StickySettings();
  ~StickySettings();

  // Populates |last_used_custom_margins| according to the last used margin
  // settings.
  void GetLastUsedMarginSettings(
      base::DictionaryValue* last_used_custom_margins) const;

  std::string* printer_name();
  std::string* printer_cloud_print_data();
  printing::ColorModels color_model() const;
  bool headers_footers() const;
  FilePath* save_path();
  printing::DuplexMode duplex_mode() const;

  // Stores color model, duplex mode, margins and headers footers.
  void Store(const base::DictionaryValue& settings);
  // Stores last used printer name.
  void StorePrinterName(const std::string& printer_name);
  // Stores last used printer cloud print data.
  void StoreCloudPrintData(const std::string& cloud_print_data);
  // Stores the last path the user used to save to pdf.
  void StoreSavePath(const FilePath& path);

 private:
  void StoreColorModel(const base::DictionaryValue& settings);
  void StoreMarginSettings(const base::DictionaryValue& settings);
  void StoreHeadersFooters(const base::DictionaryValue& settings);
  void StoreDuplexMode(const base::DictionaryValue& settings);

  scoped_ptr<FilePath> save_path_;
  scoped_ptr<std::string> printer_name_;
  scoped_ptr<std::string> printer_cloud_print_data_;
  printing::ColorModels color_model_;
  printing::MarginType margins_type_;
  scoped_ptr<printing::PageSizeMargins> page_size_margins_;
  bool headers_footers_;
  printing::DuplexMode duplex_mode_;

  friend class ::PrintPreviewHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(::PrintPreviewHandlerTest, StickyMarginsCustom);
  FRIEND_TEST_ALL_PREFIXES(::PrintPreviewHandlerTest, StickyMarginsDefault);
  FRIEND_TEST_ALL_PREFIXES(::PrintPreviewHandlerTest,
                           StickyMarginsCustomThenDefault);
  FRIEND_TEST_ALL_PREFIXES(::PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsCustom);
  FRIEND_TEST_ALL_PREFIXES(::PrintPreviewHandlerTest,
                           GetLastUsedMarginSettingsDefault);
};

} // namespace printing

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_STICKY_SETTINGS_H_
