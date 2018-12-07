// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/dark_mode_handler.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace {

using ui::NativeTheme;

}  // namespace

DarkModeHandler::~DarkModeHandler() {}

// static
void DarkModeHandler::Initialize(content::WebUI* web_ui,
                                 content::WebUIDataSource* source) {
  InitializeInternal(web_ui, source, NativeTheme::GetInstanceForNativeUi(),
                     Profile::FromWebUI(web_ui));
}

// static
void DarkModeHandler::InitializeInternal(content::WebUI* web_ui,
                                         content::WebUIDataSource* source,
                                         NativeTheme* theme,
                                         Profile* profile) {
  auto handler = base::WrapUnique(new DarkModeHandler(theme, profile));
  source->AddLocalizedStrings(*handler->GetDataSourceUpdate());
  handler->source_name_ = source->GetSource();
  web_ui->AddMessageHandler(std::move(handler));
}

DarkModeHandler::DarkModeHandler(NativeTheme* theme, Profile* profile)
    : theme_(theme),
      profile_(profile),
      using_dark_(IsDarkModeEnabled()),
      observer_(this) {}

void DarkModeHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "observeDarkMode",
      base::BindRepeating(&DarkModeHandler::HandleObserveDarkMode,
                          base::Unretained(this)));
}

void DarkModeHandler::OnNativeThemeUpdated(NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, theme_);
  NotifyIfChanged();
}

void DarkModeHandler::OnJavascriptDisallowed() {
  observer_.RemoveAll();
}

void DarkModeHandler::HandleObserveDarkMode(const base::ListValue* /*args*/) {
  AllowJavascript();
  observer_.Add(theme_);
}

bool DarkModeHandler::IsDarkModeEnabled() const {
  return base::FeatureList::IsEnabled(features::kWebUIDarkMode) &&
         theme_->SystemDarkModeEnabled();
}

std::unique_ptr<base::DictionaryValue> DarkModeHandler::GetDataSourceUpdate()
    const {
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetKey("dark", base::Value(using_dark_ ? "dark" : ""));
  update->SetKey("darkMode", base::Value(using_dark_));
  return update;
}

void DarkModeHandler::NotifyIfChanged() {
  bool use_dark = IsDarkModeEnabled();
  if (use_dark == using_dark_)
    return;
  using_dark_ = use_dark;
  FireWebUIListener("dark-mode-changed", base::Value(use_dark));
  content::WebUIDataSource::Update(profile_, source_name_,
                                   GetDataSourceUpdate());
}
