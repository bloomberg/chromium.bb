// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/themes_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/manifest.h"

using sync_datatype_helper::test;

namespace {

// Make a name to pass to an extension helper.
std::string MakeName(int index) {
  return "faketheme" + base::IntToString(index);
}

ThemeService* GetThemeService(Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile);
}

}  // namespace

namespace themes_helper {

std::string GetCustomTheme(int index) {
  return crx_file::id_util::GenerateId(MakeName(index));
}

std::string GetThemeID(Profile* profile) {
  return GetThemeService(profile)->GetThemeID();
}

bool UsingCustomTheme(Profile* profile) {
  return GetThemeID(profile) != ThemeService::kDefaultThemeID;
}

bool UsingDefaultTheme(Profile* profile) {
  return GetThemeService(profile)->UsingDefaultTheme();
}

bool UsingSystemTheme(Profile* profile) {
  return GetThemeService(profile)->UsingSystemTheme();
}

bool ThemeIsPendingInstall(Profile* profile, const std::string& id) {
  return SyncExtensionHelper::GetInstance()->
      IsExtensionPendingInstallForSync(profile, id);
}

void UseCustomTheme(Profile* profile, int index) {
  SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, MakeName(index), extensions::Manifest::TYPE_THEME);
}

void UseDefaultTheme(Profile* profile) {
  GetThemeService(profile)->UseDefaultTheme();
}

void UseSystemTheme(Profile* profile) {
  GetThemeService(profile)->UseSystemTheme();
}

namespace {

// Helper to wait until the specified theme is pending for install on the
// specified profile.
//
// The themes sync integration tests don't actually install any custom themes,
// but they do occasionally check that the ThemeService attempts to install
// synced themes.
class ThemePendingInstallChecker : public StatusChangeChecker,
                                   public content::NotificationObserver {
 public:
  ThemePendingInstallChecker(Profile* profile, const std::string& theme);
  virtual ~ThemePendingInstallChecker();

  // Implementation of StatusChangeChecker.
  virtual std::string GetDebugMessage() const OVERRIDE;
  virtual bool IsExitConditionSatisfied() OVERRIDE;

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Waits until the condition to be met or a timeout occurs.
  void Wait();

 private:
  Profile* profile_;
  const std::string& theme_;

  content::NotificationRegistrar registrar_;
};

ThemePendingInstallChecker::ThemePendingInstallChecker(Profile* profile,
                                                       const std::string& theme)
    : profile_(profile), theme_(theme) {
}

ThemePendingInstallChecker::~ThemePendingInstallChecker() {
}

std::string ThemePendingInstallChecker::GetDebugMessage() const {
  return base::StringPrintf("Waiting for pending theme to be '%s'",
                            theme_.c_str());
}

bool ThemePendingInstallChecker::IsExitConditionSatisfied() {
  return ThemeIsPendingInstall(profile_, theme_);
}

void ThemePendingInstallChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED, type);
  CheckExitCondition();
}

void ThemePendingInstallChecker::Wait() {
  // We'll check to see if the condition is met whenever the extension system
  // tries to contact the web store.
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UPDATING_STARTED,
                 content::Source<Profile>(profile_));

  if (IsExitConditionSatisfied()) {
    return;
  }

  StartBlockingWait();
}

}  // namespace

bool AwaitThemeIsPendingInstall(Profile* profile, const std::string& theme) {
  ThemePendingInstallChecker checker(profile, theme);
  checker.Wait();
  return !checker.TimedOut();
}

namespace {

// Helper to wait until a given condition is met, checking every time the
// current theme changes.
//
// The |exit_condition_| closure may be invoked zero or more times.
class ThemeConditionChecker : public StatusChangeChecker,
                              public content::NotificationObserver {
 public:
  ThemeConditionChecker(Profile* profile,
                        const std::string& debug_message_,
                        base::Callback<bool(ThemeService*)> exit_condition);
  virtual ~ThemeConditionChecker();

  // Implementation of StatusChangeChecker.
  virtual std::string GetDebugMessage() const OVERRIDE;
  virtual bool IsExitConditionSatisfied() OVERRIDE;

  // Implementation of content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Waits until the condition to be met or a timeout occurs.
  void Wait();

 private:
  Profile* profile_;
  const std::string debug_message_;
  base::Callback<bool(ThemeService*)> exit_condition_;

  content::NotificationRegistrar registrar_;
};

ThemeConditionChecker::ThemeConditionChecker(
    Profile* profile,
    const std::string& debug_message,
    base::Callback<bool(ThemeService*)> exit_condition)
    : profile_(profile),
      debug_message_(debug_message),
      exit_condition_(exit_condition) {
}

ThemeConditionChecker::~ThemeConditionChecker() {
}

std::string ThemeConditionChecker::GetDebugMessage() const {
  return debug_message_;
}

bool ThemeConditionChecker::IsExitConditionSatisfied() {
  return exit_condition_.Run(GetThemeService(profile_));
}

void ThemeConditionChecker::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  CheckExitCondition();
}

void ThemeConditionChecker::Wait() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(GetThemeService(profile_)));

  if (IsExitConditionSatisfied()) {
    return;
  }

  StartBlockingWait();
}

// Helper function to let us bind this functionality into a base::Callback.
bool UsingSystemThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingSystemTheme();
}

// Helper function to let us bind this functionality into a base::Callback.
bool UsingDefaultThemeFunc(ThemeService* theme_service) {
  return theme_service->UsingDefaultTheme();
}

}  // namespace

bool AwaitUsingSystemTheme(Profile* profile) {
  ThemeConditionChecker checker(
      profile,
      std::string("Waiting until profile is using system theme"),
      base::Bind(&UsingSystemThemeFunc));
  checker.Wait();
  return !checker.TimedOut();
}

bool AwaitUsingDefaultTheme(Profile* profile) {
  ThemeConditionChecker checker(
      profile,
      std::string("Waiting until profile is using default theme"),
      base::Bind(&UsingDefaultThemeFunc));
  checker.Wait();
  return !checker.TimedOut();
}

}  // namespace themes_helper
