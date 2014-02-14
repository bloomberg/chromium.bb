// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/runtime/runtime_api.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/omaha_query_params/omaha_query_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/runtime.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/isolated_context.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#endif

using content::BrowserContext;

namespace GetPlatformInfo = extensions::api::runtime::GetPlatformInfo;

namespace extensions {

namespace runtime = api::runtime;

namespace {

const char kNoBackgroundPageError[] = "You do not have a background page.";
const char kPageLoadError[] = "Background page failed to load.";
const char kInstallReason[] = "reason";
const char kInstallReasonChromeUpdate[] = "chrome_update";
const char kInstallReasonUpdate[] = "update";
const char kInstallReasonInstall[] = "install";
const char kInstallPreviousVersion[] = "previousVersion";
const char kInvalidUrlError[] = "Invalid URL.";
const char kUpdatesDisabledError[] = "Autoupdate is not enabled.";
const char kUpdateFound[] = "update_available";
const char kUpdateNotFound[] = "no_update";
const char kUpdateThrottled[] = "throttled";

// A preference key storing the url loaded when an extension is uninstalled.
const char kUninstallUrl[] = "uninstall_url";

// The name of the directory to be returned by getPackageDirectoryEntry. This
// particular value does not matter to user code, but is chosen for consistency
// with the equivalent Pepper API.
const char kPackageDirectoryPath[] = "crxfs";

void DispatchOnStartupEventImpl(BrowserContext* browser_context,
                                const std::string& extension_id,
                                bool first_call,
                                ExtensionHost* host) {
  // A NULL host from the LazyBackgroundTaskQueue means the page failed to
  // load. Give up.
  if (!host && !first_call)
    return;

  // Don't send onStartup events to incognito browser contexts.
  if (browser_context->IsOffTheRecord())
    return;

  if (ExtensionsBrowserClient::Get()->IsShuttingDown() ||
      !ExtensionsBrowserClient::Get()->IsValidContext(browser_context))
    return;
  ExtensionSystem* system = ExtensionSystem::Get(browser_context);
  if (!system)
    return;

  // If this is a persistent background page, we want to wait for it to load
  // (it might not be ready, since this is startup). But only enqueue once.
  // If it fails to load the first time, don't bother trying again.
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
          extension_id);
  if (extension && BackgroundInfo::HasPersistentBackgroundPage(extension) &&
      first_call &&
      system->lazy_background_task_queue()->
          ShouldEnqueueTask(browser_context, extension)) {
    system->lazy_background_task_queue()->AddPendingTask(
        browser_context, extension_id,
        base::Bind(&DispatchOnStartupEventImpl,
                   browser_context, extension_id, false));
    return;
  }

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  scoped_ptr<Event> event(new Event(runtime::OnStartup::kEventName,
                                    event_args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

void SetUninstallURL(ExtensionPrefs* prefs,
                     const std::string& extension_id,
                     const std::string& url_string) {
  prefs->UpdateExtensionPref(extension_id,
                             kUninstallUrl,
                             new base::StringValue(url_string));
}

#if defined(ENABLE_EXTENSIONS)
std::string GetUninstallURL(ExtensionPrefs* prefs,
                            const std::string& extension_id) {
  std::string url_string;
  prefs->ReadPrefAsString(extension_id, kUninstallUrl, &url_string);
  return url_string;
}
#endif  // defined(ENABLE_EXTENSIONS)

}  // namespace

///////////////////////////////////////////////////////////////////////////////

RuntimeAPI::RuntimeAPI(content::BrowserContext* context)
    : browser_context_(context),
      dispatch_chrome_updated_event_(false),
      registered_for_updates_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<BrowserContext>(context));

  // Check if registered events are up-to-date. We can only do this once
  // per browser context, since it updates internal state when called.
  dispatch_chrome_updated_event_ =
      ExtensionsBrowserClient::Get()->DidVersionUpdate(browser_context_);
}

RuntimeAPI::~RuntimeAPI() {
  if (registered_for_updates_) {
    ExtensionSystem::Get(browser_context_)->
        extension_service()->RemoveUpdateObserver(this);
  }
}

void RuntimeAPI::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      OnExtensionsReady();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      OnExtensionLoaded(extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      OnExtensionInstalled(extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      OnExtensionUninstalled(extension);
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

void RuntimeAPI::OnExtensionsReady() {
  // We're done restarting Chrome after an update.
  dispatch_chrome_updated_event_ = false;

  registered_for_updates_ = true;

  ExtensionSystem::Get(browser_context_)->extension_service()->
      AddUpdateObserver(this);
}

void RuntimeAPI::OnExtensionLoaded(const Extension* extension) {
  if (!dispatch_chrome_updated_event_)
    return;

  // Dispatch the onInstalled event with reason "chrome_update".
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RuntimeEventRouter::DispatchOnInstalledEvent,
                 browser_context_,
                 extension->id(),
                 Version(),
                 true));
}

void RuntimeAPI::OnExtensionInstalled(const Extension* extension) {
  // Ephemeral apps are not considered to be installed and do not receive
  // the onInstalled() event.
  if (extension->is_ephemeral())
    return;

  // Get the previous version to check if this is an upgrade.
  ExtensionService* service = ExtensionSystem::Get(
      browser_context_)->extension_service();
  const Extension* old = service->GetExtensionById(extension->id(), true);
  Version old_version;
  if (old)
    old_version = *old->version();

  // Dispatch the onInstalled event.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&RuntimeEventRouter::DispatchOnInstalledEvent,
                 browser_context_,
                 extension->id(),
                 old_version,
                 false));

}

void RuntimeAPI::OnExtensionUninstalled(const Extension* extension) {
  // Ephemeral apps are not considered to be installed, so the uninstall URL
  // is not invoked when they are removed.
  if (extension->is_ephemeral())
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  RuntimeEventRouter::OnExtensionUninstalled(profile, extension->id());
}

void RuntimeAPI::OnAppUpdateAvailable(const Extension* extension) {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
      profile, extension->id(), extension->manifest()->value());
}

void RuntimeAPI::OnChromeUpdateAvailable() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(profile);
}

///////////////////////////////////////////////////////////////////////////////

// static
void RuntimeEventRouter::DispatchOnStartupEvent(
    content::BrowserContext* context, const std::string& extension_id) {
  DispatchOnStartupEventImpl(context, extension_id, true, NULL);
}

// static
void RuntimeEventRouter::DispatchOnInstalledEvent(
    content::BrowserContext* context,
    const std::string& extension_id,
    const Version& old_version,
    bool chrome_updated) {
  if (!ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  base::DictionaryValue* info = new base::DictionaryValue();
  event_args->Append(info);
  if (old_version.IsValid()) {
    info->SetString(kInstallReason, kInstallReasonUpdate);
    info->SetString(kInstallPreviousVersion, old_version.GetString());
  } else if (chrome_updated) {
    info->SetString(kInstallReason, kInstallReasonChromeUpdate);
  } else {
    info->SetString(kInstallReason, kInstallReasonInstall);
  }
  DCHECK(system->event_router());
  scoped_ptr<Event> event(new Event(runtime::OnInstalled::kEventName,
                                    event_args.Pass()));
  system->event_router()->DispatchEventWithLazyListener(extension_id,
                                                        event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
    Profile* profile,
    const std::string& extension_id,
    const base::DictionaryValue* manifest) {
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(manifest->DeepCopy());
  DCHECK(system->event_router());
  scoped_ptr<Event> event(new Event(runtime::OnUpdateAvailable::kEventName,
                                    args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(
    Profile* profile) {
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  DCHECK(system->event_router());
  scoped_ptr<Event> event(new Event(
      runtime::OnBrowserUpdateAvailable::kEventName, args.Pass()));
  system->event_router()->BroadcastEvent(event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnRestartRequiredEvent(
    Profile* profile,
    const std::string& app_id,
    api::runtime::OnRestartRequired::Reason reason) {
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system)
    return;

  scoped_ptr<Event> event(
      new Event(runtime::OnRestartRequired::kEventName,
                api::runtime::OnRestartRequired::Create(reason)));

  DCHECK(system->event_router());
  system->event_router()->DispatchEventToExtension(app_id, event.Pass());
}

// static
void RuntimeEventRouter::OnExtensionUninstalled(
    Profile* profile,
    const std::string& extension_id) {
#if defined(ENABLE_EXTENSIONS)
  GURL uninstall_url(GetUninstallURL(ExtensionPrefs::Get(profile),
                                     extension_id));

  if (uninstall_url.is_empty())
    return;

  Browser* browser = chrome::FindLastActiveWithProfile(profile,
      chrome::GetActiveDesktop());
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile,
                                                chrome::GetActiveDesktop()));

  chrome::NavigateParams params(browser, uninstall_url,
                                content::PAGE_TRANSITION_CLIENT_REDIRECT);
  params.disposition = NEW_FOREGROUND_TAB;
  params.user_gesture = false;
  chrome::Navigate(&params);
#endif  // defined(ENABLE_EXTENSIONS)
}

bool RuntimeGetBackgroundPageFunction::RunImpl() {
  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ExtensionHost* host = system->process_manager()->
      GetBackgroundHostForExtension(extension_id());
  if (system->lazy_background_task_queue()->ShouldEnqueueTask(GetProfile(),
                                                              GetExtension())) {
    system->lazy_background_task_queue()->AddPendingTask(
        GetProfile(),
        extension_id(),
        base::Bind(&RuntimeGetBackgroundPageFunction::OnPageLoaded, this));
  } else if (host) {
    OnPageLoaded(host);
  } else {
    error_ = kNoBackgroundPageError;
    return false;
  }

  return true;
}

void RuntimeGetBackgroundPageFunction::OnPageLoaded(ExtensionHost* host) {
  if (host) {
    SendResponse(true);
  } else {
    error_ = kPageLoadError;
    SendResponse(false);
  }
}

bool RuntimeSetUninstallURLFunction::RunImpl() {
  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url_string));

  GURL url(url_string);
  if (!url.is_valid()) {
    error_ = ErrorUtils::FormatErrorMessage(kInvalidUrlError, url_string);
    return false;
  }

  SetUninstallURL(
      ExtensionPrefs::Get(GetProfile()), extension_id(), url_string);
  return true;
}

bool RuntimeReloadFunction::RunImpl() {
  // We can't call ReloadExtension directly, since when this method finishes
  // it tries to decrease the reference count for the extension, which fails
  // if the extension has already been reloaded; so instead we post a task.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionService::ReloadExtension,
                 GetProfile()->GetExtensionService()->AsWeakPtr(),
                 extension_id()));
  return true;
}

RuntimeRequestUpdateCheckFunction::RuntimeRequestUpdateCheckFunction() {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UPDATE_FOUND,
                 content::NotificationService::AllSources());
}

bool RuntimeRequestUpdateCheckFunction::RunImpl() {
  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ExtensionService* service = system->extension_service();
  ExtensionUpdater* updater = service->updater();
  if (!updater) {
    error_ = kUpdatesDisabledError;
    return false;
  }

  did_reply_ = false;
  if (!updater->CheckExtensionSoon(extension_id(), base::Bind(
      &RuntimeRequestUpdateCheckFunction::CheckComplete, this))) {
    did_reply_ = true;
    SetResult(new base::StringValue(kUpdateThrottled));
    SendResponse(true);
  }
  return true;
}

void RuntimeRequestUpdateCheckFunction::CheckComplete() {
  if (did_reply_)
    return;

  did_reply_ = true;

  // Since no UPDATE_FOUND notification was seen, this generally would mean
  // that no update is found, but a previous update check might have already
  // queued up an update, so check for that here to make sure we return the
  // right value.
  ExtensionSystem* system = ExtensionSystem::Get(GetProfile());
  ExtensionService* service = system->extension_service();
  const Extension* update = service->GetPendingExtensionUpdate(extension_id());
  if (update) {
    ReplyUpdateFound(update->VersionString());
  } else {
    SetResult(new base::StringValue(kUpdateNotFound));
  }
  SendResponse(true);
}

void RuntimeRequestUpdateCheckFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (did_reply_)
    return;

  DCHECK(type == chrome::NOTIFICATION_EXTENSION_UPDATE_FOUND);
  typedef const std::pair<std::string, Version> UpdateDetails;
  const std::string& id = content::Details<UpdateDetails>(details)->first;
  const Version& version = content::Details<UpdateDetails>(details)->second;
  if (id == extension_id()) {
    ReplyUpdateFound(version.GetString());
  }
}

void RuntimeRequestUpdateCheckFunction::ReplyUpdateFound(
    const std::string& version) {
  did_reply_ = true;
  results_.reset(new base::ListValue);
  results_->AppendString(kUpdateFound);
  base::DictionaryValue* details = new base::DictionaryValue;
  results_->Append(details);
  details->SetString("version", version);
  SendResponse(true);
}

bool RuntimeRestartFunction::RunImpl() {
#if defined(OS_CHROMEOS)
  if (chromeos::UserManager::Get()->IsLoggedInAsKioskApp()) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestRestart();
    return true;
  }
#endif
  SetError("Function available only for ChromeOS kiosk mode.");
  return false;
}

bool RuntimeGetPlatformInfoFunction::RunImpl() {
  GetPlatformInfo::Results::PlatformInfo info;

  const char* os = chrome::OmahaQueryParams::GetOS();
  if (strcmp(os, "mac") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_MAC_;
  } else if (strcmp(os, "win") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_WIN_;
  } else if (strcmp(os, "android") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_ANDROID_;
  } else if (strcmp(os, "cros") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_CROS_;
  } else if (strcmp(os, "linux") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_LINUX_;
  } else if (strcmp(os, "openbsd") == 0) {
    info.os = GetPlatformInfo::Results::PlatformInfo::OS_OPENBSD_;
  } else {
    NOTREACHED();
    return false;
  }

  const char* arch = chrome::OmahaQueryParams::GetArch();
  if (strcmp(arch, "arm") == 0) {
    info.arch = GetPlatformInfo::Results::PlatformInfo::ARCH_ARM;
  } else if (strcmp(arch, "x86") == 0) {
    info.arch = GetPlatformInfo::Results::PlatformInfo::ARCH_X86_32;
  } else if (strcmp(arch, "x64") == 0) {
    info.arch = GetPlatformInfo::Results::PlatformInfo::ARCH_X86_64;
  } else {
    NOTREACHED();
    return false;
  }

  const char* nacl_arch = chrome::OmahaQueryParams::GetNaclArch();
  if (strcmp(nacl_arch, "arm") == 0) {
    info.nacl_arch = GetPlatformInfo::Results::PlatformInfo::NACL_ARCH_ARM;
  } else if (strcmp(nacl_arch, "x86-32") == 0) {
    info.nacl_arch = GetPlatformInfo::Results::PlatformInfo::NACL_ARCH_X86_32;
  } else if (strcmp(nacl_arch, "x86-64") == 0) {
    info.nacl_arch = GetPlatformInfo::Results::PlatformInfo::NACL_ARCH_X86_64;
  } else {
    NOTREACHED();
    return false;
  }

  results_ = GetPlatformInfo::Results::Create(info);
  return true;
}

bool RuntimeGetPackageDirectoryEntryFunction::RunImpl() {
  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  std::string relative_path = kPackageDirectoryPath;
  base::FilePath path = extension_->path();
  std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
      fileapi::kFileSystemTypeNativeLocal, path, &relative_path);

  int renderer_id = render_view_host_->GetProcess()->GetID();
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(renderer_id, filesystem_id);
  base::DictionaryValue* dict = new base::DictionaryValue();
  SetResult(dict);
  dict->SetString("fileSystemId", filesystem_id);
  dict->SetString("baseName", relative_path);
  return true;
}

}   // namespace extensions
