// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/base/locale_util.h"

#include <utility>
#include <vector>

#include "base/task_scheduler/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/platform_font_linux.h"

namespace chromeos {

namespace {

struct SwitchLanguageData {
  SwitchLanguageData(const std::string& locale,
                     const bool enable_locale_keyboard_layouts,
                     const bool login_layouts_only,
                     const locale_util::SwitchLanguageCallback& callback,
                     Profile* profile)
      : callback(callback),
        result(locale, std::string(), false),
        enable_locale_keyboard_layouts(enable_locale_keyboard_layouts),
        login_layouts_only(login_layouts_only),
        profile(profile) {}

  const locale_util::SwitchLanguageCallback callback;

  locale_util::LanguageSwitchResult result;
  const bool enable_locale_keyboard_layouts;
  const bool login_layouts_only;
  Profile* profile;
};

// Runs on SequencedWorkerPool thread under PostTaskAndReply().
// So data is owned by "Reply" part of PostTaskAndReply() process.
void SwitchLanguageDoReloadLocale(SwitchLanguageData* data) {
  DCHECK(!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  data->result.loaded_locale =
      ResourceBundle::GetSharedInstance().ReloadLocaleResources(
          data->result.requested_locale);

  data->result.success = !data->result.loaded_locale.empty();
}

// Callback after SwitchLanguageDoReloadLocale() back in UI thread.
void FinishSwitchLanguage(std::unique_ptr<SwitchLanguageData> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (data->result.success) {
    g_browser_process->SetApplicationLocale(data->result.loaded_locale);

    if (data->enable_locale_keyboard_layouts) {
      input_method::InputMethodManager* manager =
          input_method::InputMethodManager::Get();
      scoped_refptr<input_method::InputMethodManager::State> ime_state =
          UserSessionManager::GetInstance()->GetDefaultIMEState(data->profile);
      if (data->login_layouts_only) {
        // Enable the hardware keyboard layouts and locale-specific layouts
        // suitable for use on the login screen. This will also switch to the
        // first hardware keyboard layout since the input method currently in
        // use may not be supported by the new locale.
        ime_state->EnableLoginLayouts(
            data->result.loaded_locale,
            manager->GetInputMethodUtil()->GetHardwareLoginInputMethodIds());
      } else {
        // Enable all hardware keyboard layouts. This will also switch to the
        // first hardware keyboard layout.
        ime_state->ReplaceEnabledInputMethods(
            manager->GetInputMethodUtil()->GetHardwareInputMethodIds());

        // Enable all locale-specific layouts.
        std::vector<std::string> input_methods;
        manager->GetInputMethodUtil()->GetInputMethodIdsFromLanguageCode(
            data->result.loaded_locale,
            input_method::kKeyboardLayoutsOnly,
            &input_methods);
        for (std::vector<std::string>::const_iterator it =
                input_methods.begin(); it != input_methods.end(); ++it) {
          ime_state->EnableInputMethod(*it);
        }
      }
    }
  }

  // The font clean up of ResourceBundle should be done on UI thread, since the
  // cached fonts are thread unsafe.
  ResourceBundle::GetSharedInstance().ReloadFonts();
  gfx::PlatformFontLinux::ReloadDefaultFont();
  if (!data->callback.is_null())
    data->callback.Run(data->result);
}

}  // namespace

namespace locale_util {

LanguageSwitchResult::LanguageSwitchResult(const std::string& requested_locale,
                                           const std::string& loaded_locale,
                                           bool success)
    : requested_locale(requested_locale),
      loaded_locale(loaded_locale),
      success(success) {
}

void SwitchLanguage(const std::string& locale,
                    const bool enable_locale_keyboard_layouts,
                    const bool login_layouts_only,
                    const SwitchLanguageCallback& callback,
                    Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<SwitchLanguageData> data(
      new SwitchLanguageData(locale, enable_locale_keyboard_layouts,
                             login_layouts_only, callback, profile));
  base::Closure reloader(
      base::Bind(&SwitchLanguageDoReloadLocale, base::Unretained(data.get())));
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, base::TaskTraits().MayBlock().WithPriority(
                     base::TaskPriority::BACKGROUND),
      reloader,
      base::Bind(&FinishSwitchLanguage, base::Passed(std::move(data))));
}

}  // namespace locale_util
}  // namespace chromeos
