// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/runtime/runtime_api.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/runtime.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/isolated_context.h"

using content::BrowserContext;

namespace extensions {

namespace runtime = core_api::runtime;

namespace {

const char kNoBackgroundPageError[] = "You do not have a background page.";
const char kPageLoadError[] = "Background page failed to load.";
const char kInstallId[] = "id";
const char kInstallReason[] = "reason";
const char kInstallReasonChromeUpdate[] = "chrome_update";
const char kInstallReasonUpdate[] = "update";
const char kInstallReasonInstall[] = "install";
const char kInstallReasonSharedModuleUpdate[] = "shared_module_update";
const char kInstallPreviousVersion[] = "previousVersion";
const char kInvalidUrlError[] = "Invalid URL.";
const char kPlatformInfoUnavailable[] = "Platform information unavailable.";

const char kUpdatesDisabledError[] = "Autoupdate is not enabled.";

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
      system->lazy_background_task_queue()->ShouldEnqueueTask(browser_context,
                                                              extension)) {
    system->lazy_background_task_queue()->AddPendingTask(
        browser_context,
        extension_id,
        base::Bind(
            &DispatchOnStartupEventImpl, browser_context, extension_id, false));
    return;
  }

  scoped_ptr<base::ListValue> event_args(new base::ListValue());
  scoped_ptr<Event> event(
      new Event(runtime::OnStartup::kEventName, event_args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

void SetUninstallURL(ExtensionPrefs* prefs,
                     const std::string& extension_id,
                     const std::string& url_string) {
  prefs->UpdateExtensionPref(
      extension_id, kUninstallUrl, new base::StringValue(url_string));
}

std::string GetUninstallURL(ExtensionPrefs* prefs,
                            const std::string& extension_id) {
  std::string url_string;
  prefs->ReadPrefAsString(extension_id, kUninstallUrl, &url_string);
  return url_string;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

static base::LazyInstance<BrowserContextKeyedAPIFactory<RuntimeAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<RuntimeAPI>* RuntimeAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

RuntimeAPI::RuntimeAPI(content::BrowserContext* context)
    : browser_context_(context), dispatch_chrome_updated_event_(false) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED,
                 content::Source<BrowserContext>(context));

  delegate_ = ExtensionsBrowserClient::Get()->CreateRuntimeAPIDelegate(
      browser_context_);

  // Check if registered events are up-to-date. We can only do this once
  // per browser context, since it updates internal state when called.
  dispatch_chrome_updated_event_ =
      ExtensionsBrowserClient::Get()->DidVersionUpdate(browser_context_);
}

RuntimeAPI::~RuntimeAPI() {
  delegate_->RemoveUpdateObserver(this);
}

void RuntimeAPI::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      OnExtensionsReady();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      OnExtensionLoaded(extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_WILL_BE_INSTALLED_DEPRECATED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      OnExtensionInstalled(extension);
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED_DEPRECATED: {
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

  delegate_->AddUpdateObserver(this);

  // RuntimeAPI is redirected in incognito, so |browser_context_| is never
  // incognito. We don't observe incognito ProcessManagers but that is OK
  // because we don't send onStartup events to incognito browser contexts.
  DCHECK(!browser_context_->IsOffTheRecord());
  // Some tests use partially constructed Profiles without a process manager.
  ExtensionSystem* extension_system = ExtensionSystem::Get(browser_context_);
  if (extension_system->process_manager())
    extension_system->process_manager()->AddObserver(this);
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
  if (util::IsEphemeralApp(extension->id(), browser_context_))
    return;

  Version old_version = delegate_->GetPreviousExtensionVersion(extension);

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
  if (util::IsEphemeralApp(extension->id(), browser_context_))
    return;

  RuntimeEventRouter::OnExtensionUninstalled(browser_context_, extension->id());
}

void RuntimeAPI::Shutdown() {
  // ExtensionSystem deletes its ProcessManager during the Shutdown() phase, so
  // the observer must be removed here and not in the RuntimeAPI destructor.
  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context_)->process_manager();
  // Some tests use partially constructed Profiles without a process manager.
  if (process_manager)
    process_manager->RemoveObserver(this);
}

void RuntimeAPI::OnAppUpdateAvailable(const Extension* extension) {
  RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
      browser_context_, extension->id(), extension->manifest()->value());
}

void RuntimeAPI::OnChromeUpdateAvailable() {
  RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(browser_context_);
}

void RuntimeAPI::OnBackgroundHostStartup(const Extension* extension) {
  RuntimeEventRouter::DispatchOnStartupEvent(browser_context_, extension->id());
}

void RuntimeAPI::ReloadExtension(const std::string& extension_id) {
  delegate_->ReloadExtension(extension_id);
}

bool RuntimeAPI::CheckForUpdates(
    const std::string& extension_id,
    const RuntimeAPIDelegate::UpdateCheckCallback& callback) {
  return delegate_->CheckForUpdates(extension_id, callback);
}

void RuntimeAPI::OpenURL(const GURL& update_url) {
  delegate_->OpenURL(update_url);
}

bool RuntimeAPI::GetPlatformInfo(runtime::PlatformInfo* info) {
  return delegate_->GetPlatformInfo(info);
}

bool RuntimeAPI::RestartDevice(std::string* error_message) {
  return delegate_->RestartDevice(error_message);
}

///////////////////////////////////////////////////////////////////////////////

// static
void RuntimeEventRouter::DispatchOnStartupEvent(
    content::BrowserContext* context,
    const std::string& extension_id) {
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
  scoped_ptr<Event> event(
      new Event(runtime::OnInstalled::kEventName, event_args.Pass()));
  system->event_router()->DispatchEventWithLazyListener(extension_id,
                                                        event.Pass());

  if (old_version.IsValid()) {
    const Extension* extension =
        ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
            extension_id);
    if (extension && SharedModuleInfo::IsSharedModule(extension)) {
      scoped_ptr<ExtensionSet> dependents =
          system->GetDependentExtensions(extension);
      for (ExtensionSet::const_iterator i = dependents->begin();
           i != dependents->end();
           i++) {
        scoped_ptr<base::ListValue> sm_event_args(new base::ListValue());
        base::DictionaryValue* sm_info = new base::DictionaryValue();
        sm_event_args->Append(sm_info);
        sm_info->SetString(kInstallReason, kInstallReasonSharedModuleUpdate);
        sm_info->SetString(kInstallPreviousVersion, old_version.GetString());
        sm_info->SetString(kInstallId, extension_id);
        scoped_ptr<Event> sm_event(
            new Event(runtime::OnInstalled::kEventName, sm_event_args.Pass()));
        system->event_router()->DispatchEventWithLazyListener((*i)->id(),
                                                              sm_event.Pass());
      }
    }
  }
}

// static
void RuntimeEventRouter::DispatchOnUpdateAvailableEvent(
    content::BrowserContext* context,
    const std::string& extension_id,
    const base::DictionaryValue* manifest) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(manifest->DeepCopy());
  DCHECK(system->event_router());
  scoped_ptr<Event> event(
      new Event(runtime::OnUpdateAvailable::kEventName, args.Pass()));
  system->event_router()->DispatchEventToExtension(extension_id, event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnBrowserUpdateAvailableEvent(
    content::BrowserContext* context) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<base::ListValue> args(new base::ListValue);
  DCHECK(system->event_router());
  scoped_ptr<Event> event(
      new Event(runtime::OnBrowserUpdateAvailable::kEventName, args.Pass()));
  system->event_router()->BroadcastEvent(event.Pass());
}

// static
void RuntimeEventRouter::DispatchOnRestartRequiredEvent(
    content::BrowserContext* context,
    const std::string& app_id,
    core_api::runtime::OnRestartRequired::Reason reason) {
  ExtensionSystem* system = ExtensionSystem::Get(context);
  if (!system)
    return;

  scoped_ptr<Event> event(
      new Event(runtime::OnRestartRequired::kEventName,
                core_api::runtime::OnRestartRequired::Create(reason)));

  DCHECK(system->event_router());
  system->event_router()->DispatchEventToExtension(app_id, event.Pass());
}

// static
void RuntimeEventRouter::OnExtensionUninstalled(
    content::BrowserContext* context,
    const std::string& extension_id) {
  GURL uninstall_url(
      GetUninstallURL(ExtensionPrefs::Get(context), extension_id));

  if (uninstall_url.is_empty())
    return;

  RuntimeAPI::GetFactoryInstance()->Get(context)->OpenURL(uninstall_url);
}

ExtensionFunction::ResponseAction RuntimeGetBackgroundPageFunction::Run() {
  ExtensionSystem* system = ExtensionSystem::Get(browser_context());
  ExtensionHost* host =
      system->process_manager()->GetBackgroundHostForExtension(extension_id());
  if (system->lazy_background_task_queue()->ShouldEnqueueTask(browser_context(),
                                                              GetExtension())) {
    system->lazy_background_task_queue()->AddPendingTask(
        browser_context(),
        extension_id(),
        base::Bind(&RuntimeGetBackgroundPageFunction::OnPageLoaded, this));
  } else if (host) {
    OnPageLoaded(host);
  } else {
    return RespondNow(Error(kNoBackgroundPageError));
  }

  return RespondLater();
}

void RuntimeGetBackgroundPageFunction::OnPageLoaded(ExtensionHost* host) {
  if (host) {
    Respond(NoArguments());
  } else {
    Respond(Error(kPageLoadError));
  }
}

ExtensionFunction::ResponseAction RuntimeSetUninstallURLFunction::Run() {
  std::string url_string;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &url_string));

  GURL url(url_string);
  if (!url.is_valid()) {
    return RespondNow(
        Error(ErrorUtils::FormatErrorMessage(kInvalidUrlError, url_string)));
  }
  SetUninstallURL(
      ExtensionPrefs::Get(browser_context()), extension_id(), url_string);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeReloadFunction::Run() {
  RuntimeAPI::GetFactoryInstance()->Get(browser_context())->ReloadExtension(
      extension_id());
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeRequestUpdateCheckFunction::Run() {
  if (!RuntimeAPI::GetFactoryInstance()
           ->Get(browser_context())
           ->CheckForUpdates(
               extension_id(),
               base::Bind(&RuntimeRequestUpdateCheckFunction::CheckComplete,
                          this))) {
    return RespondNow(Error(kUpdatesDisabledError));
  }
  return RespondLater();
}

void RuntimeRequestUpdateCheckFunction::CheckComplete(
    const RuntimeAPIDelegate::UpdateCheckResult& result) {
  if (result.success) {
    base::DictionaryValue* details = new base::DictionaryValue;
    details->SetString("version", result.version);
    Respond(TwoArguments(new base::StringValue(result.response), details));
  } else {
    // HMM(kalman): Why does !success not imply Error()?
    Respond(OneArgument(new base::StringValue(result.response)));
  }
}

ExtensionFunction::ResponseAction RuntimeRestartFunction::Run() {
  std::string message;
  bool result =
      RuntimeAPI::GetFactoryInstance()->Get(browser_context())->RestartDevice(
          &message);
  if (!result) {
    return RespondNow(Error(message));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction RuntimeGetPlatformInfoFunction::Run() {
  runtime::PlatformInfo info;
  if (!RuntimeAPI::GetFactoryInstance()
           ->Get(browser_context())
           ->GetPlatformInfo(&info)) {
    return RespondNow(Error(kPlatformInfoUnavailable));
  }
  return RespondNow(
      ArgumentList(runtime::GetPlatformInfo::Results::Create(info)));
}

ExtensionFunction::ResponseAction
RuntimeGetPackageDirectoryEntryFunction::Run() {
  fileapi::IsolatedContext* isolated_context =
      fileapi::IsolatedContext::GetInstance();
  DCHECK(isolated_context);

  std::string relative_path = kPackageDirectoryPath;
  base::FilePath path = extension_->path();
  std::string filesystem_id = isolated_context->RegisterFileSystemForPath(
      fileapi::kFileSystemTypeNativeLocal, std::string(), path, &relative_path);

  int renderer_id = render_view_host_->GetProcess()->GetID();
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  policy->GrantReadFileSystem(renderer_id, filesystem_id);
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("fileSystemId", filesystem_id);
  dict->SetString("baseName", relative_path);
  return RespondNow(OneArgument(dict));
}

}  // namespace extensions
