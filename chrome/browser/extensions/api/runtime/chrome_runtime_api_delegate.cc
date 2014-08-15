// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.h"

#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_warning_service.h"
#include "chrome/browser/extensions/extension_warning_set.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/omaha_query_params/omaha_query_params.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/api/runtime.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/user_manager/user_manager.h"
#endif

using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::ExtensionUpdater;

using extensions::core_api::runtime::PlatformInfo;

namespace {

const char kUpdateThrottled[] = "throttled";
const char kUpdateNotFound[] = "no_update";
const char kUpdateFound[] = "update_available";

// If an extension reloads itself within this many miliseconds of reloading
// itself, the reload is considered suspiciously fast.
const int kFastReloadTime = 10000;

// After this many suspiciously fast consecutive reloads, an extension will get
// disabled.
const int kFastReloadCount = 5;

}  // namespace

ChromeRuntimeAPIDelegate::ChromeRuntimeAPIDelegate(
    content::BrowserContext* context)
    : browser_context_(context), registered_for_updates_(false) {
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                 content::NotificationService::AllSources());
}

ChromeRuntimeAPIDelegate::~ChromeRuntimeAPIDelegate() {
}

void ChromeRuntimeAPIDelegate::AddUpdateObserver(
    extensions::UpdateObserver* observer) {
  registered_for_updates_ = true;
  ExtensionSystem::Get(browser_context_)
      ->extension_service()
      ->AddUpdateObserver(observer);
}

void ChromeRuntimeAPIDelegate::RemoveUpdateObserver(
    extensions::UpdateObserver* observer) {
  if (registered_for_updates_) {
    ExtensionSystem::Get(browser_context_)
        ->extension_service()
        ->RemoveUpdateObserver(observer);
  }
}

base::Version ChromeRuntimeAPIDelegate::GetPreviousExtensionVersion(
    const Extension* extension) {
  // Get the previous version to check if this is an upgrade.
  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();
  const Extension* old = service->GetExtensionById(extension->id(), true);
  if (old)
    return *old->version();
  return base::Version();
}

void ChromeRuntimeAPIDelegate::ReloadExtension(
    const std::string& extension_id) {
  std::pair<base::TimeTicks, int>& reload_info =
      last_reload_time_[extension_id];
  base::TimeTicks now = base::TimeTicks::Now();
  if (reload_info.first.is_null() ||
      (now - reload_info.first).InMilliseconds() > kFastReloadTime) {
    reload_info.second = 0;
  } else {
    reload_info.second++;
  }
  if (!reload_info.first.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Extensions.RuntimeReloadTime",
                             now - reload_info.first);
  }
  UMA_HISTOGRAM_COUNTS_100("Extensions.RuntimeReloadFastCount",
                           reload_info.second);
  reload_info.first = now;

  ExtensionService* service =
      ExtensionSystem::Get(browser_context_)->extension_service();
  if (reload_info.second >= kFastReloadCount) {
    // Unloading an extension clears all warnings, so first terminate the
    // extension, and then add the warning. Since this is called from an
    // extension function unloading the extension has to be done
    // asynchronously. Fortunately PostTask guarentees FIFO order so just
    // post both tasks.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionService::TerminateExtension,
                   service->AsWeakPtr(),
                   extension_id));
    extensions::ExtensionWarningSet warnings;
    warnings.insert(
        extensions::ExtensionWarning::CreateReloadTooFrequentWarning(
            extension_id));
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&extensions::ExtensionWarningService::NotifyWarningsOnUI,
                   browser_context_,
                   warnings));
  } else {
    // We can't call ReloadExtension directly, since when this method finishes
    // it tries to decrease the reference count for the extension, which fails
    // if the extension has already been reloaded; so instead we post a task.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ExtensionService::ReloadExtension,
                   service->AsWeakPtr(),
                   extension_id));
  }
}

bool ChromeRuntimeAPIDelegate::CheckForUpdates(
    const std::string& extension_id,
    const UpdateCheckCallback& callback) {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context_);
  ExtensionService* service = system->extension_service();
  ExtensionUpdater* updater = service->updater();
  if (!updater) {
    return false;
  }
  if (!updater->CheckExtensionSoon(
          extension_id,
          base::Bind(&ChromeRuntimeAPIDelegate::UpdateCheckComplete,
                     base::Unretained(this),
                     extension_id))) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, UpdateCheckResult(true, kUpdateThrottled, "")));
  } else {
    UpdateCallbackList& callbacks = pending_update_checks_[extension_id];
    callbacks.push_back(callback);
  }
  return true;
}

void ChromeRuntimeAPIDelegate::OpenURL(const GURL& uninstall_url) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  Browser* browser =
      chrome::FindLastActiveWithProfile(profile, chrome::GetActiveDesktop());
  if (!browser)
    browser =
        new Browser(Browser::CreateParams(profile, chrome::GetActiveDesktop()));

  chrome::NavigateParams params(
      browser, uninstall_url, content::PAGE_TRANSITION_CLIENT_REDIRECT);
  params.disposition = NEW_FOREGROUND_TAB;
  params.user_gesture = false;
  chrome::Navigate(&params);
}

bool ChromeRuntimeAPIDelegate::GetPlatformInfo(PlatformInfo* info) {
  const char* os = omaha_query_params::OmahaQueryParams::GetOS();
  if (strcmp(os, "mac") == 0) {
    info->os = PlatformInfo::OS_MAC_;
  } else if (strcmp(os, "win") == 0) {
    info->os = PlatformInfo::OS_WIN_;
  } else if (strcmp(os, "cros") == 0) {
    info->os = PlatformInfo::OS_CROS_;
  } else if (strcmp(os, "linux") == 0) {
    info->os = PlatformInfo::OS_LINUX_;
  } else if (strcmp(os, "openbsd") == 0) {
    info->os = PlatformInfo::OS_OPENBSD_;
  } else {
    NOTREACHED();
    return false;
  }

  const char* arch = omaha_query_params::OmahaQueryParams::GetArch();
  if (strcmp(arch, "arm") == 0) {
    info->arch = PlatformInfo::ARCH_ARM;
  } else if (strcmp(arch, "x86") == 0) {
    info->arch = PlatformInfo::ARCH_X86_32;
  } else if (strcmp(arch, "x64") == 0) {
    info->arch = PlatformInfo::ARCH_X86_64;
  } else {
    NOTREACHED();
    return false;
  }

  const char* nacl_arch = omaha_query_params::OmahaQueryParams::GetNaclArch();
  if (strcmp(nacl_arch, "arm") == 0) {
    info->nacl_arch = PlatformInfo::NACL_ARCH_ARM;
  } else if (strcmp(nacl_arch, "x86-32") == 0) {
    info->nacl_arch = PlatformInfo::NACL_ARCH_X86_32;
  } else if (strcmp(nacl_arch, "x86-64") == 0) {
    info->nacl_arch = PlatformInfo::NACL_ARCH_X86_64;
  } else {
    NOTREACHED();
    return false;
  }

  return true;
}

bool ChromeRuntimeAPIDelegate::RestartDevice(std::string* error_message) {
#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::Get()->IsLoggedInAsKioskApp()) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestRestart();
    return true;
  }
#endif
  *error_message = "Function available only for ChromeOS kiosk mode.";
  return false;
}

void ChromeRuntimeAPIDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == extensions::NOTIFICATION_EXTENSION_UPDATE_FOUND);
  typedef const std::pair<std::string, Version> UpdateDetails;
  const std::string& id = content::Details<UpdateDetails>(details)->first;
  const Version& version = content::Details<UpdateDetails>(details)->second;
  if (version.IsValid()) {
    CallUpdateCallbacks(
        id, UpdateCheckResult(true, kUpdateFound, version.GetString()));
  }
}

void ChromeRuntimeAPIDelegate::UpdateCheckComplete(
    const std::string& extension_id) {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context_);
  ExtensionService* service = system->extension_service();
  const Extension* update = service->GetPendingExtensionUpdate(extension_id);
  if (update) {
    CallUpdateCallbacks(
        extension_id,
        UpdateCheckResult(true, kUpdateFound, update->VersionString()));
  } else {
    CallUpdateCallbacks(extension_id,
                        UpdateCheckResult(true, kUpdateNotFound, ""));
  }
}

void ChromeRuntimeAPIDelegate::CallUpdateCallbacks(
    const std::string& extension_id,
    const UpdateCheckResult& result) {
  UpdateCallbackList callbacks = pending_update_checks_[extension_id];
  pending_update_checks_.erase(extension_id);
  for (UpdateCallbackList::const_iterator iter = callbacks.begin();
       iter != callbacks.end();
       ++iter) {
    const UpdateCheckCallback& callback = *iter;
    callback.Run(result);
  }
}
