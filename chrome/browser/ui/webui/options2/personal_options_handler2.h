// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS2_PERSONAL_OPTIONS_HANDLER2_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS2_PERSONAL_OPTIONS_HANDLER2_H_
#pragma once

#include "base/basictypes.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/options2/options_ui2.h"
#if defined(OS_CHROMEOS)
#include "content/public/browser/notification_registrar.h"
#endif

namespace options2 {

// Chrome personal options page UI handler.
class PersonalOptionsHandler : public OptionsPageUIHandler {
 public:
  PersonalOptionsHandler();
  virtual ~PersonalOptionsHandler();

  // OptionsPageUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  void ObserveThemeChanged();
  void ThemesReset(const ListValue* args);
#if defined(TOOLKIT_GTK)
  void ThemesSetGTK(const ListValue* args);
#endif

#if defined(OS_CHROMEOS)
  void UpdateAccountPicture();
  content::NotificationRegistrar registrar_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PersonalOptionsHandler);
};

}  // namespace options2

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS2_PERSONAL_OPTIONS_HANDLER2_H_
