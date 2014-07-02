// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/declarative_content/content_rules_registry.h"
#include "chrome/browser/extensions/api/webstore/webstore_api.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/script_executor.h"
#include "chrome/browser/extensions/webstore_inline_installer.h"
#include "chrome/browser/extensions/webstore_inline_installer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/chrome_extension_messages.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/icons_handler.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/web_applications/web_app_win.h"
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::RenderViewHost;
using content::WebContents;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::TabHelper);

namespace extensions {

TabHelper::ScriptExecutionObserver::ScriptExecutionObserver(
    TabHelper* tab_helper)
    : tab_helper_(tab_helper) {
  tab_helper_->AddScriptExecutionObserver(this);
}

TabHelper::ScriptExecutionObserver::ScriptExecutionObserver()
    : tab_helper_(NULL) {
}

TabHelper::ScriptExecutionObserver::~ScriptExecutionObserver() {
  if (tab_helper_)
    tab_helper_->RemoveScriptExecutionObserver(this);
}

TabHelper::TabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      extension_app_(NULL),
      extension_function_dispatcher_(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          this),
      pending_web_app_action_(NONE),
      last_committed_page_id_(-1),
      update_shortcut_on_load_complete_(false),
      script_executor_(
          new ScriptExecutor(web_contents, &script_execution_observers_)),
      location_bar_controller_(new LocationBarController(web_contents)),
      image_loader_ptr_factory_(this),
      webstore_inline_installer_factory_(new WebstoreInlineInstallerFactory()) {
  // The ActiveTabPermissionManager requires a session ID; ensure this
  // WebContents has one.
  SessionTabHelper::CreateForWebContents(web_contents);
  if (web_contents->GetRenderViewHost())
    SetTabId(web_contents->GetRenderViewHost());
  active_tab_permission_granter_.reset(new ActiveTabPermissionGranter(
      web_contents,
      SessionID::IdForTab(web_contents),
      Profile::FromBrowserContext(web_contents->GetBrowserContext())));

  // If more classes need to listen to global content script activity, then
  // a separate routing class with an observer interface should be written.
  profile_ = Profile::FromBrowserContext(web_contents->GetBrowserContext());

  AddScriptExecutionObserver(ActivityLog::GetInstance(profile_));

  registrar_.Add(this,
                 content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(
                     &web_contents->GetController()));
}

TabHelper::~TabHelper() {
  RemoveScriptExecutionObserver(ActivityLog::GetInstance(profile_));
}

void TabHelper::CreateApplicationShortcuts() {
  DCHECK(CanCreateApplicationShortcuts());
  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  GetApplicationInfo(CREATE_SHORTCUT);
}

void TabHelper::CreateHostedAppFromWebContents() {
  DCHECK(CanCreateBookmarkApp());
  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  GetApplicationInfo(CREATE_HOSTED_APP);
}

bool TabHelper::CanCreateApplicationShortcuts() const {
#if defined(OS_MACOSX)
  return false;
#else
  return web_app::IsValidUrl(web_contents()->GetURL()) &&
      pending_web_app_action_ == NONE;
#endif
}

bool TabHelper::CanCreateBookmarkApp() const {
#if defined(OS_MACOSX)
  return false;
#else
  return IsValidBookmarkAppUrl(web_contents()->GetURL()) &&
         pending_web_app_action_ == NONE;
#endif
}

void TabHelper::SetExtensionApp(const Extension* extension) {
  DCHECK(!extension || AppLaunchInfo::GetFullLaunchURL(extension).is_valid());
  if (extension_app_ == extension)
    return;

  extension_app_ = extension;

  UpdateExtensionAppIcon(extension_app_);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_CONTENTS_APPLICATION_EXTENSION_CHANGED,
      content::Source<TabHelper>(this),
      content::NotificationService::NoDetails());
}

void TabHelper::SetExtensionAppById(const std::string& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension)
    SetExtensionApp(extension);
}

void TabHelper::SetExtensionAppIconById(const std::string& extension_app_id) {
  const Extension* extension = GetExtension(extension_app_id);
  if (extension)
    UpdateExtensionAppIcon(extension);
}

SkBitmap* TabHelper::GetExtensionAppIcon() {
  if (extension_app_icon_.empty())
    return NULL;

  return &extension_app_icon_;
}

void TabHelper::FinishCreateBookmarkApp(
    const Extension* extension,
    const WebApplicationInfo& web_app_info) {
  pending_web_app_action_ = NONE;

  // There was an error with downloading the icons or installing the app.
  if (!extension)
    return;

#if defined(OS_CHROMEOS)
  ChromeLauncherController::instance()->PinAppWithID(extension->id());
#endif

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (browser) {
    browser->window()->ShowBookmarkAppBubble(web_app_info, extension->id());
  }
}

void TabHelper::RenderViewCreated(RenderViewHost* render_view_host) {
  SetTabId(render_view_host);
}

void TabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (ExtensionSystem::Get(profile_)->extension_service() &&
      RulesRegistryService::Get(profile_)) {
    RulesRegistryService::Get(profile_)->content_rules_registry()->
        DidNavigateMainFrame(web_contents(), details, params);
  }

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);
  const ExtensionSet& enabled_extensions = registry->enabled_extensions();

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps)) {
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser && browser->is_app()) {
      SetExtensionApp(registry->GetExtensionById(
          web_app::GetExtensionIdFromApplicationName(browser->app_name()),
          ExtensionRegistry::EVERYTHING));
    } else {
      UpdateExtensionAppIcon(
          enabled_extensions.GetExtensionOrAppByURL(params.url));
    }
  } else {
    UpdateExtensionAppIcon(
        enabled_extensions.GetExtensionOrAppByURL(params.url));
  }

  if (details.is_in_page)
    return;

  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(Profile::FromBrowserContext(context));
  for (ExtensionSet::const_iterator it = enabled_extensions.begin();
       it != enabled_extensions.end();
       ++it) {
    ExtensionAction* browser_action =
        extension_action_manager->GetBrowserAction(*it->get());
    if (browser_action) {
      browser_action->ClearAllValuesForTab(SessionID::IdForTab(web_contents()));
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
          content::Source<ExtensionAction>(browser_action),
          content::NotificationService::NoDetails());
    }
  }
}

bool TabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeExtensionHostMsg_DidGetApplicationInfo,
                        OnDidGetApplicationInfo)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_InlineWebstoreInstall,
                        OnInlineWebstoreInstall)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_GetAppInstallState,
                        OnGetAppInstallState);
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ContentScriptsExecuting,
                        OnContentScriptsExecuting)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_OnWatchedPageChange,
                        OnWatchedPageChange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool TabHelper::OnMessageReceived(const IPC::Message& message,
                                  content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TabHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DetailedConsoleMessageAdded,
                        OnDetailedConsoleMessageAdded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabHelper::DidCloneToNewWebContents(WebContents* old_web_contents,
                                         WebContents* new_web_contents) {
  // When the WebContents that this is attached to is cloned, give the new clone
  // a TabHelper and copy state over.
  CreateForWebContents(new_web_contents);
  TabHelper* new_helper = FromWebContents(new_web_contents);

  new_helper->SetExtensionApp(extension_app());
  new_helper->extension_app_icon_ = extension_app_icon_;
}

void TabHelper::OnDidGetApplicationInfo(const WebApplicationInfo& info) {
#if !defined(OS_MACOSX)
  web_app_info_ = info;

  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || last_committed_page_id_ != entry->GetPageID())
    return;
  last_committed_page_id_ = -1;

  switch (pending_web_app_action_) {
    case CREATE_SHORTCUT: {
      chrome::ShowCreateWebAppShortcutsDialog(
          web_contents()->GetTopLevelNativeWindow(),
          web_contents());
      break;
    }
    case CREATE_HOSTED_APP: {
      if (web_app_info_.app_url.is_empty())
        web_app_info_.app_url = web_contents()->GetURL();

      if (web_app_info_.title.empty())
        web_app_info_.title = web_contents()->GetTitle();
      if (web_app_info_.title.empty())
        web_app_info_.title = base::UTF8ToUTF16(web_app_info_.app_url.spec());

      bookmark_app_helper_.reset(new BookmarkAppHelper(
          ExtensionSystem::Get(profile_)->extension_service(),
          web_app_info_, web_contents()));
      bookmark_app_helper_->Create(base::Bind(
          &TabHelper::FinishCreateBookmarkApp, base::Unretained(this)));
      break;
    }
    case UPDATE_SHORTCUT: {
      web_app::UpdateShortcutForTabContents(web_contents());
      break;
    }
    default:
      NOTREACHED();
      break;
  }

  // The hosted app action will be cleared once the installation completes or
  // fails.
  if (pending_web_app_action_ != CREATE_HOSTED_APP)
    pending_web_app_action_ = NONE;
#endif
}

void TabHelper::OnInlineWebstoreInstall(int install_id,
                                        int return_route_id,
                                        const std::string& webstore_item_id,
                                        const GURL& requestor_url,
                                        int listeners_mask) {
  // Check that the listener is reasonable. We should never get anything other
  // than an install stage listener, a download listener, or both.
  if ((listeners_mask & ~(api::webstore::INSTALL_STAGE_LISTENER |
                          api::webstore::DOWNLOAD_PROGRESS_LISTENER)) != 0) {
    NOTREACHED();
    return;
  }
  // Inform the Webstore API that an inline install is happening, in case the
  // page requested status updates.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  WebstoreAPI::Get(profile)->OnInlineInstallStart(
      return_route_id, this, webstore_item_id, listeners_mask);

  WebstoreStandaloneInstaller::Callback callback =
      base::Bind(&TabHelper::OnInlineInstallComplete, base::Unretained(this),
                 install_id, return_route_id);
  scoped_refptr<WebstoreInlineInstaller> installer(
      webstore_inline_installer_factory_->CreateInstaller(
          web_contents(),
          webstore_item_id,
          requestor_url,
          callback));
  installer->BeginInstall();
}

void TabHelper::OnGetAppInstallState(const GURL& requestor_url,
                                     int return_route_id,
                                     int callback_id) {
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents()->GetBrowserContext());
  const ExtensionSet& extensions = registry->enabled_extensions();
  const ExtensionSet& disabled_extensions = registry->disabled_extensions();

  std::string state;
  if (extensions.GetHostedAppByURL(requestor_url))
    state = extension_misc::kAppStateInstalled;
  else if (disabled_extensions.GetHostedAppByURL(requestor_url))
    state = extension_misc::kAppStateDisabled;
  else
    state = extension_misc::kAppStateNotInstalled;

  Send(new ExtensionMsg_GetAppInstallStateResponse(
      return_route_id, state, callback_id));
}

void TabHelper::OnRequest(const ExtensionHostMsg_Request_Params& request) {
  extension_function_dispatcher_.Dispatch(request,
                                          web_contents()->GetRenderViewHost());
}

void TabHelper::OnContentScriptsExecuting(
    const ScriptExecutionObserver::ExecutingScriptsMap& executing_scripts_map,
    const GURL& on_url) {
  FOR_EACH_OBSERVER(ScriptExecutionObserver, script_execution_observers_,
                    OnScriptsExecuted(web_contents(),
                                      executing_scripts_map,
                                      on_url));
}

void TabHelper::OnWatchedPageChange(
    const std::vector<std::string>& css_selectors) {
  if (ExtensionSystem::Get(profile_)->extension_service() &&
      RulesRegistryService::Get(profile_)) {
    RulesRegistryService::Get(profile_)->content_rules_registry()->Apply(
        web_contents(), css_selectors);
  }
}

void TabHelper::OnDetailedConsoleMessageAdded(
    const base::string16& message,
    const base::string16& source,
    const StackTrace& stack_trace,
    int32 severity_level) {
  if (IsSourceFromAnExtension(source)) {
    content::RenderViewHost* rvh = web_contents()->GetRenderViewHost();
    ErrorConsole::Get(profile_)->ReportError(
        scoped_ptr<ExtensionError>(new RuntimeError(
            extension_app_ ? extension_app_->id() : std::string(),
            profile_->IsOffTheRecord(),
            source,
            message,
            stack_trace,
            web_contents() ?
                web_contents()->GetLastCommittedURL() : GURL::EmptyGURL(),
            static_cast<logging::LogSeverity>(severity_level),
            rvh->GetRoutingID(),
            rvh->GetProcess()->GetID())));
  }
}

const Extension* TabHelper::GetExtension(const std::string& extension_app_id) {
  if (extension_app_id.empty())
    return NULL;

  content::BrowserContext* context = web_contents()->GetBrowserContext();
  return ExtensionRegistry::Get(context)->enabled_extensions().GetByID(
      extension_app_id);
}

void TabHelper::UpdateExtensionAppIcon(const Extension* extension) {
  extension_app_icon_.reset();
  // Ensure previously enqueued callbacks are ignored.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Enqueue OnImageLoaded callback.
  if (extension) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    ImageLoader* loader = ImageLoader::Get(profile);
    loader->LoadImageAsync(
        extension,
        IconsInfo::GetIconResource(extension,
                                   extension_misc::EXTENSION_ICON_SMALL,
                                   ExtensionIconSet::MATCH_BIGGER),
        gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                  extension_misc::EXTENSION_ICON_SMALL),
        base::Bind(&TabHelper::OnImageLoaded,
                   image_loader_ptr_factory_.GetWeakPtr()));
  }
}

void TabHelper::SetAppIcon(const SkBitmap& app_icon) {
  extension_app_icon_ = app_icon;
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TITLE);
}

void TabHelper::SetWebstoreInlineInstallerFactoryForTests(
    WebstoreInlineInstallerFactory* factory) {
  webstore_inline_installer_factory_.reset(factory);
}

void TabHelper::OnImageLoaded(const gfx::Image& image) {
  if (!image.IsEmpty()) {
    extension_app_icon_ = *image.ToSkBitmap();
    web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TAB);
  }
}

WindowController* TabHelper::GetExtensionWindowController() const  {
  return ExtensionTabUtil::GetWindowControllerOfTab(web_contents());
}

void TabHelper::OnInlineInstallComplete(int install_id,
                                        int return_route_id,
                                        bool success,
                                        const std::string& error) {
  Send(new ExtensionMsg_InlineWebstoreInstallResponse(
      return_route_id, install_id, success, success ? std::string() : error));
}

WebContents* TabHelper::GetAssociatedWebContents() const {
  return web_contents();
}

void TabHelper::GetApplicationInfo(WebAppAction action) {
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  pending_web_app_action_ = action;
  last_committed_page_id_ = entry->GetPageID();

  Send(new ChromeExtensionMsg_GetApplicationInfo(routing_id()));
}

void TabHelper::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_LOAD_STOP, type);
  const NavigationController& controller =
      *content::Source<NavigationController>(source).ptr();
  DCHECK_EQ(controller.GetWebContents(), web_contents());

  if (update_shortcut_on_load_complete_) {
    update_shortcut_on_load_complete_ = false;
    // Schedule a shortcut update when web application info is available if
    // last committed entry is not NULL. Last committed entry could be NULL
    // when an interstitial page is injected (e.g. bad https certificate,
    // malware site etc). When this happens, we abort the shortcut update.
    if (controller.GetLastCommittedEntry())
      GetApplicationInfo(UPDATE_SHORTCUT);
  }
}

void TabHelper::SetTabId(RenderViewHost* render_view_host) {
  render_view_host->Send(
      new ExtensionMsg_SetTabId(render_view_host->GetRoutingID(),
                                SessionID::IdForTab(web_contents())));
}

}  // namespace extensions
