// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"

#include "base/file_path.h"
#include "base/values.h"
#include "printing/page_size_margins.h"

namespace printing {

StickySettings::StickySettings()
    : color_model_(printing::UNKNOWN_COLOR_MODEL),
      margins_type_(printing::DEFAULT_MARGINS),
      headers_footers_(true),
      duplex_mode_(printing::UNKNOWN_DUPLEX_MODE) {}

StickySettings::~StickySettings() {}

void StickySettings::GetLastUsedMarginSettings(
    base::DictionaryValue* custom_margins) const {
  custom_margins->SetInteger(printing::kSettingMarginsType,
                             margins_type_);
  if (page_size_margins_.get()) {
    custom_margins->SetDouble(printing::kSettingMarginTop,
                              page_size_margins_->margin_top);
    custom_margins->SetDouble(printing::kSettingMarginBottom,
                              page_size_margins_->margin_bottom);
    custom_margins->SetDouble(printing::kSettingMarginLeft,
                              page_size_margins_->margin_left);
    custom_margins->SetDouble(printing::kSettingMarginRight,
                              page_size_margins_->margin_right);
  }
}

void StickySettings::StoreColorModel(
    const base::DictionaryValue& settings) {
  int color_model;
  if (!settings.GetInteger(printing::kSettingColor, &color_model))
    color_model = printing::GRAY;
  color_model_ = static_cast<printing::ColorModels>(color_model);
}

void StickySettings::StoreMarginSettings(
    const base::DictionaryValue& settings) {
  int margin_type;
  if (!settings.GetInteger(printing::kSettingMarginsType, &margin_type))
    margin_type = printing::DEFAULT_MARGINS;
  margins_type_ = static_cast<printing::MarginType>(margin_type);
  if (margins_type_ == printing::CUSTOM_MARGINS) {
    if (!page_size_margins_.get())
      page_size_margins_.reset(new printing::PageSizeMargins());
    GetCustomMarginsFromJobSettings(
        settings, page_size_margins_.get());
  }
}

void StickySettings::StoreHeadersFooters(
    const base::DictionaryValue& settings) {
  settings.GetBoolean(
      printing::kSettingHeaderFooterEnabled, &headers_footers_);
}

void StickySettings::Store(const base::DictionaryValue& settings) {
  // Storing last used color model.
  StoreColorModel(settings);
  // Storing last used duplex mode.
  StoreDuplexMode(settings);

  bool is_modifiable = false;
  settings.GetBoolean(printing::kSettingPreviewModifiable, &is_modifiable);
  if (is_modifiable) {
    // Storing last used margin settings.
    StoreMarginSettings(settings);
    // Storing last used header and footer setting.
    StoreHeadersFooters(settings);
  }
}

void StickySettings::StoreDuplexMode(
    const base::DictionaryValue& settings) {
  int duplex_mode = printing::UNKNOWN_DUPLEX_MODE;
  settings.GetInteger(printing::kSettingDuplexMode, &duplex_mode);
  duplex_mode_ = static_cast<printing::DuplexMode>(duplex_mode);
}

void StickySettings::StorePrinterName(const std::string& printer_name) {
  printer_name_.reset(new std::string(printer_name));
}

void StickySettings::StoreCloudPrintData(const std::string& data) {
  printer_cloud_print_data_.reset(new std::string(data));
}

void StickySettings::StoreSavePath(const FilePath& path) {
  save_path_.reset(new FilePath(path));
}

std::string* StickySettings::printer_name() {
  return printer_name_.get();
}

std::string* StickySettings::printer_cloud_print_data() {
  return printer_cloud_print_data_.get();
}

printing::ColorModels StickySettings::color_model() const {
  return color_model_;
}

bool StickySettings::headers_footers() const {
  return headers_footers_;
}

FilePath* StickySettings::save_path() {
  return save_path_.get();
}

printing::DuplexMode StickySettings::duplex_mode() const {
  return duplex_mode_;
}

} // namespace printing
