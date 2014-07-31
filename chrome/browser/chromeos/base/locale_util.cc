// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/locale_util.h"

#include <vector>

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
                     const bool enable_locale_keyboard_layouts,
                     const bool login_layouts_only,
                     scoped_ptr<locale_util::SwitchLanguageCallback> callback)
      : callback(callback.Pass()),
        locale(locale),
        enable_locale_keyboard_layouts(enable_locale_keyboard_layouts),
        login_layouts_only(login_layouts_only),
        success(false) {}

  scoped_ptr<locale_util::SwitchLanguageCallback> callback;

  const std::string locale;
  const bool enable_locale_keyboard_layouts;
  const bool login_layouts_only;
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

    if (data->enable_locale_keyboard_layouts) {
      input_method::InputMethodManager* manager =
          input_method::InputMethodManager::Get();
      if (data->login_layouts_only) {
        // Enable the hardware keyboard layouts and locale-specific layouts
        // suitable for use on the login screen. This will also switch to the
        // first hardware keyboard layout since the input method currently in
        // use may not be supported by the new locale.
        manager->EnableLoginLayouts(
            data->loaded_locale,
            manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
      } else {
        // Enable all hardware keyboard layouts. This will also switch to the
        // first hardware keyboard layout.
        manager->ReplaceEnabledInputMethods(
            manager->GetInputMethodUtil()->GetHardwareInputMethodIds());

        // Enable all locale-specific layouts.
        std::vector<std::string> input_methods;
        manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
            data->loaded_locale,
            input_method::kKeyboardLayoutsOnly,
            &input_methods);
        for (std::vector<std::string>::const_iterator it =
                input_methods.begin(); it != input_methods.end(); ++it) {
          manager->EnableInputMethod(*it);
        }
      }
    }
  }
  gfx::PlatformFontPango::ReloadDefaultFont();
  if (data->callback)
    data->callback->Run(data->locale, data->loaded_locale, data->success);
}

}  // namespace

namespace locale_util {

void SwitchLanguage(const std::string& locale,
                    const bool enable_locale_keyboard_layouts,
                    const bool login_layouts_only,
                    scoped_ptr<SwitchLanguageCallback> callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  scoped_ptr<SwitchLanguageData> data(
      new SwitchLanguageData(locale,
                             enable_locale_keyboard_layouts,
                             login_layouts_only,
                             callback.Pass()));
  base::Closure reloader(
      base::Bind(&SwitchLanguageDoReloadLocale, base::Unretained(data.get())));
  content::BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      reloader,
      base::Bind(&FinishSwitchLanguage, base::Passed(data.Pass())));
}

}  // namespace locale_util
}  // namespace chromeos
