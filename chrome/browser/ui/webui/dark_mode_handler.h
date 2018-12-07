// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace base {
class ListValue;
}  // namespace base

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace ui {
class NativeTheme;
}  // namespace ui

class Profile;

class DarkModeHandler : public content::WebUIMessageHandler,
                        public ui::NativeThemeObserver {
 public:
  ~DarkModeHandler() override;

  // Sets load-time constants on |source|. This handles a flicker-free initial
  // page load (i.e. $i18n{dark}, loadTimeData.getBoolean('darkMode')). Adds a
  // DarkModeHandler to |web_ui| if WebUI dark mode enhancements are enabled.
  // If enabled, continually updates |source| by name if dark mode changes. This
  // is so page refreshes will have fresh, correct data. Neither |web_ui| nor
  // |source| may be nullptr.
  static void Initialize(content::WebUI* web_ui,
                         content::WebUIDataSource* source);

 protected:
  // Protected for testing.
  static void InitializeInternal(content::WebUI* web_ui,
                                 content::WebUIDataSource* source,
                                 ui::NativeTheme* theme,
                                 Profile* profile);

  explicit DarkModeHandler(ui::NativeTheme* theme, Profile* profile);

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptDisallowed() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Handles the "observeDarkMode" message. No arguments. Protected for testing.
  void HandleObserveDarkMode(const base::ListValue* args);

  // Generates a dictionary with "dark" and "darkMode" i18n keys based on
  // |using_dark_|. Called initialize and on each change for notifications.
  std::unique_ptr<base::DictionaryValue> GetDataSourceUpdate() const;

  // Whether the feature is enabled and the system is in dark mode.
  bool IsDarkModeEnabled() const;

  // Fire a webui listener notification if dark mode actually changed.
  void NotifyIfChanged();

  // The theme to query/watch for changes. Injected for testing.
  ui::NativeTheme* const theme_;

  // Profile to update data sources on. Injected for testing.
  Profile* const profile_;

  // Whether or not this page is using dark mode.
  bool using_dark_;

  ScopedObserver<ui::NativeTheme, DarkModeHandler> observer_;

  // Populated if feature is enabled.
  std::string source_name_;

  DISALLOW_COPY_AND_ASSIGN(DarkModeHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_DARK_MODE_HANDLER_H_
