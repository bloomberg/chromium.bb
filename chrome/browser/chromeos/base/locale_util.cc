// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/locale_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chromeos/ime/input_method_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font_pango.h"

namespace chromeos {

namespace {

struct SwitchLanguageData {
  SwitchLanguageData(const std::string& locale,
                     const bool enableLocaleKeyboardLayouts,
                     scoped_ptr<locale_util::SwitchLanguageCallback> callback)
      : callback(callback.Pass()),
        locale(locale),
        enableLocaleKeyboardLayouts(enableLocaleKeyboardLayouts),
        success(false) {}

  scoped_ptr<locale_util::SwitchLanguageCallback> callback;

  const std::string locale;
  const bool enableLocaleKeyboardLayouts;
  std::string loaded_locale;
  bool success;
};

// Runs on SequencedWorkerPool thread under PostTaskAndReply().
// So data is owned by "Reply" part of PostTaskAndReply() process.
void SwitchLanguageDoReloadLocale(SwitchLanguageData* data) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  data->loaded_locale =
      ResourceBundle::GetSharedInstance().ReloadLocaleResources(data->locale);

  data->success = !data->loaded_locale.empty();

  ResourceBundle::GetSharedInstance().ReloadFonts();
}

// Callback after SwitchLanguageDoReloadLocale() back in UI thread.
void FinishSwitchLanguage(scoped_ptr<SwitchLanguageData> data) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (data->success) {
    g_browser_process->SetApplicationLocale(data->loaded_locale);

    if (data->enableLocaleKeyboardLayouts) {
      // If we have switched the locale, enable the keyboard layouts that
      // are necessary for the new locale.  Change the current input method
      // to the hardware keyboard layout since the input method currently in
      // use may not be supported by the new locale (3rd parameter).
      input_method::InputMethodManager* manager =
          input_method::InputMethodManager::Get();
      manager->EnableLayouts(
          data->locale,
          manager->GetInputMethodUtil()->GetHardwareInputMethodId());
    }
  }
  gfx::PlatformFontPango::ReloadDefaultFont();
  if (data->callback)
    data->callback->Run(data->locale, data->loaded_locale, data->success);
}

}  // namespace

namespace locale_util {

void SwitchLanguage(const std::string& locale,
                    bool enableLocaleKeyboardLayouts,
                    scoped_ptr<SwitchLanguageCallback> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<SwitchLanguageData> data(new SwitchLanguageData(
      locale, enableLocaleKeyboardLayouts, callback.Pass()));
  base::Closure reloader(
      base::Bind(&SwitchLanguageDoReloadLocale, base::Unretained(data.get())));
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      reloader,
      base::Bind(&FinishSwitchLanguage, base::Passed(data.Pass())));
}

}  // namespace locale_util
}  // namespace chromeos
