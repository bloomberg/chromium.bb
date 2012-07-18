// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/sticky_settings.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "printing/page_size_margins.h"

namespace printing {

const char kSettingSavePath[] = "savePath";
const char kSettingCloudPrintData[] = "cloudPrintData";

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

void StickySettings::SaveInPrefs(PrefService* prefs) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrintSettingsReset))
    return;
  DCHECK(prefs);
  if (prefs) {
    scoped_ptr<DictionaryValue> value(new DictionaryValue);
    if (save_path_.get())
      value->SetString(printing::kSettingSavePath, save_path_->value());
    if (printer_name_.get())
      value->SetString(printing::kSettingPrinterName, *printer_name_);
    if (printer_cloud_print_data_.get())
      value->SetString(printing::kSettingCloudPrintData,
          *printer_cloud_print_data_);
    value->SetInteger(printing::kSettingColor, color_model_);
    value->SetInteger(printing::kSettingMarginsType, margins_type_);
    if (margins_type_ == printing::CUSTOM_MARGINS) {
      if (page_size_margins_.get()) {
        value->SetDouble(printing::kSettingMarginTop,
                         page_size_margins_->margin_top);
        value->SetDouble(printing::kSettingMarginBottom,
                         page_size_margins_->margin_bottom);
        value->SetDouble(printing::kSettingMarginLeft,
                         page_size_margins_->margin_left);
        value->SetDouble(printing::kSettingMarginRight,
                         page_size_margins_->margin_right);
      }
    }
    value->SetBoolean(printing::kSettingHeaderFooterEnabled, headers_footers_);
    value->SetInteger(printing::kSettingDuplexMode, duplex_mode_);
    prefs->Set(prefs::kPrintPreviewStickySettings, *value);
  }
}

void StickySettings::RestoreFromPrefs(PrefService* prefs) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrintSettingsReset))
    return;
  DCHECK(prefs);
  if (prefs) {
    const DictionaryValue* value =
        prefs->GetDictionary(prefs::kPrintPreviewStickySettings);

    FilePath::StringType save_path;
    if (value->GetString(printing::kSettingSavePath, &save_path))
      save_path_.reset(new FilePath(save_path));
    std::string buffer;
    if (value->GetString(printing::kSettingPrinterName, &buffer))
      StorePrinterName(buffer);
    if (value->GetString(printing::kSettingCloudPrintData, &buffer))
      StoreCloudPrintData(buffer);
    int int_buffer;
    if (value->GetInteger(printing::kSettingColor, &int_buffer))
      color_model_ = static_cast<printing::ColorModels>(int_buffer);
    if (value->GetInteger(printing::kSettingMarginsType, &int_buffer))
      margins_type_ = static_cast<printing::MarginType>(int_buffer);
    if (margins_type_ == printing::CUSTOM_MARGINS) {
      if (!page_size_margins_.get())
        page_size_margins_.reset(new PageSizeMargins);
      if (page_size_margins_.get()) {
        value->GetDouble(printing::kSettingMarginTop,
                         &page_size_margins_->margin_top);
        value->GetDouble(printing::kSettingMarginBottom,
                         &page_size_margins_->margin_bottom);
        value->GetDouble(printing::kSettingMarginLeft,
                         &page_size_margins_->margin_left);
        value->GetDouble(printing::kSettingMarginRight,
                         &page_size_margins_->margin_right);
      }
    }
    value->GetBoolean(printing::kSettingHeaderFooterEnabled, &headers_footers_);
    if (value->GetInteger(printing::kSettingDuplexMode, &int_buffer))
      duplex_mode_ = static_cast<printing::DuplexMode>(int_buffer);
  }
}

void StickySettings::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(prefs::kPrintPreviewStickySettings,
                                PrefService::UNSYNCABLE_PREF);
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
