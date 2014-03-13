// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/tab_helper.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/declarative/rules_registry_service.h"
#include "chrome/browser/extensions/api/declarative_content/content_rules_registry.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/favicon_downloader.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/page_action_controller.h"
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
#include "chrome/browser/ui/web_applications/web_app_ui.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
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
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/frame_navigate_params.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/feature_switch.h"
#include "skia/ext/image_operations.h"
#include "skia/ext/platform_canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
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

// static
std::map<int, SkBitmap> TabHelper::ConstrainBitmapsToSizes(
    const std::vector<SkBitmap>& bitmaps,
    const std::set<int>& sizes) {
  std::map<int, SkBitmap> output_bitmaps;
  std::map<int, SkBitmap> ordered_bitmaps;
  for (std::vector<SkBitmap>::const_iterator it = bitmaps.begin();
       it != bitmaps.end(); ++it) {
    DCHECK(it->width() == it->height());
    ordered_bitmaps[it->width()] = *it;
  }

  std::set<int>::const_iterator sizes_it = sizes.begin();
  std::map<int, SkBitmap>::const_iterator bitmaps_it = ordered_bitmaps.begin();
  while (sizes_it != sizes.end() && bitmaps_it != ordered_bitmaps.end()) {
    int size = *sizes_it;
    // Find the closest not-smaller bitmap.
    bitmaps_it = ordered_bitmaps.lower_bound(size);
    ++sizes_it;
    // Ensure the bitmap is valid and smaller than the next allowed size.
    if (bitmaps_it != ordered_bitmaps.end() &&
        (sizes_it == sizes.end() || bitmaps_it->second.width() < *sizes_it)) {
      // Resize the bitmap if it does not exactly match the desired size.
      output_bitmaps[size] = bitmaps_it->second.width() == size
          ? bitmaps_it->second
          : skia::ImageOperations::Resize(
                bitmaps_it->second, skia::ImageOperations::RESIZE_LANCZOS3,
                size, size);
    }
  }
  return output_bitmaps;
}

// static
void TabHelper::GenerateContainerIcon(std::map<int, SkBitmap>* bitmaps,
                                      int output_size) {
  std::map<int, SkBitmap>::const_iterator it =
      bitmaps->lower_bound(output_size);
  // Do nothing if there is no icon smaller than the desired size or there is
  // already an icon of |output_size|.
  if (it == bitmaps->begin() || bitmaps->count(output_size))
    return;

  --it;
  // This is the biggest icon smaller than |output_size|.
  const SkBitmap& base_icon = it->second;

  const size_t kBorderRadius = 5;
  const size_t kColorStripHeight = 3;
  const SkColor kBorderColor = 0xFFD5D5D5;
  const SkColor kBackgroundColor = 0xFFFFFFFF;

  // Create a separate canvas for the color strip.
  SkBitmap color_strip_bitmap;
  color_strip_bitmap.allocN32Pixels(output_size, output_size);
  SkCanvas color_strip_canvas(color_strip_bitmap);
  color_strip_canvas.clear(SK_ColorTRANSPARENT);

  // Draw a rounded rect of the |base_icon|'s dominant color.
  SkPaint color_strip_paint;
  color_strip_paint.setFlags(SkPaint::kAntiAlias_Flag);
  color_strip_paint.setColor(
      color_utils::CalculateKMeanColorOfBitmap(base_icon));
  color_strip_canvas.drawRoundRect(SkRect::MakeWH(output_size, output_size),
                                   kBorderRadius,
                                   kBorderRadius,
                                   color_strip_paint);

  // Erase the top of the rounded rect to leave a color strip.
  SkPaint clear_paint;
  clear_paint.setColor(SK_ColorTRANSPARENT);
  clear_paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  color_strip_canvas.drawRect(
      SkRect::MakeWH(output_size, output_size - kColorStripHeight),
      clear_paint);

  // Draw each element to an output canvas.
  SkBitmap generated_icon;
  generated_icon.allocN32Pixels(output_size, output_size);
  SkCanvas generated_icon_canvas(generated_icon);
  generated_icon_canvas.clear(SK_ColorTRANSPARENT);

  // Draw the background.
  SkPaint background_paint;
  background_paint.setColor(kBackgroundColor);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  generated_icon_canvas.drawRoundRect(SkRect::MakeWH(output_size, output_size),
                                      kBorderRadius,
                                      kBorderRadius,
                                      background_paint);

  // Draw the color strip.
  generated_icon_canvas.drawBitmap(color_strip_bitmap, 0, 0);

  // Draw the border.
  SkPaint border_paint;
  border_paint.setColor(kBorderColor);
  border_paint.setStyle(SkPaint::kStroke_Style);
  border_paint.setFlags(SkPaint::kAntiAlias_Flag);
  generated_icon_canvas.drawRoundRect(SkRect::MakeWH(output_size, output_size),
                                      kBorderRadius,
                                      kBorderRadius,
                                      border_paint);

  // Draw the centered base icon to the output canvas.
  generated_icon_canvas.drawBitmap(base_icon,
                                   (output_size - base_icon.width()) / 2,
                                   (output_size - base_icon.height()) / 2);

  generated_icon.deepCopyTo(&(*bitmaps)[output_size]);
}

TabHelper::TabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      extension_app_(NULL),
      extension_function_dispatcher_(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()), this),
      pending_web_app_action_(NONE),
      script_executor_(new ScriptExecutor(web_contents,
                                          &script_execution_observers_)),
      location_bar_controller_(new PageActionController(web_contents)),
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

#if defined(ENABLE_EXTENSIONS)
  AddScriptExecutionObserver(ActivityLog::GetInstance(profile_));
#endif

  registrar_.Add(this,
                 content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(
                     &web_contents->GetController()));

  registrar_.Add(this,
                 chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                 content::Source<CrxInstaller>(NULL));

  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                 content::NotificationService::AllSources());
}

TabHelper::~TabHelper() {
#if defined(ENABLE_EXTENSIONS)
  RemoveScriptExecutionObserver(ActivityLog::GetInstance(profile_));
#endif
}

void TabHelper::CreateApplicationShortcuts() {
  DCHECK(CanCreateApplicationShortcuts());
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  pending_web_app_action_ = CREATE_SHORTCUT;

  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  GetApplicationInfo(entry->GetPageID());
}

void TabHelper::CreateHostedAppFromWebContents() {
  DCHECK(CanCreateApplicationShortcuts());
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return;

  pending_web_app_action_ = CREATE_HOSTED_APP;

  // Start fetching web app info for CreateApplicationShortcut dialog and show
  // the dialog when the data is available in OnDidGetApplicationInfo.
  GetApplicationInfo(entry->GetPageID());
}

bool TabHelper::CanCreateApplicationShortcuts() const {
#if defined(OS_MACOSX)
  return false;
#else
  return web_app::IsValidUrl(web_contents()->GetURL()) &&
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

void TabHelper::RenderViewCreated(RenderViewHost* render_view_host) {
  SetTabId(render_view_host);
}

void TabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
#if defined(ENABLE_EXTENSIONS)
  if (ExtensionSystem::Get(profile_)->extension_service() &&
      RulesRegistryService::Get(profile_)) {
    RulesRegistryService::Get(profile_)->content_rules_registry()->
        DidNavigateMainFrame(web_contents(), details, params);
  }
#endif  // defined(ENABLE_EXTENSIONS)

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* service = profile->GetExtensionService();
  if (!service)
    return;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps)) {
#if !defined(OS_ANDROID)
    Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
    if (browser && browser->is_app()) {
      SetExtensionApp(service->GetInstalledExtension(
          web_app::GetExtensionIdFromApplicationName(browser->app_name())));
    } else {
      UpdateExtensionAppIcon(service->GetInstalledExtensionByUrl(params.url));
    }
#endif
  } else {
    UpdateExtensionAppIcon(service->GetInstalledExtensionByUrl(params.url));
  }

  if (details.is_in_page)
    return;

  ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(profile);
  for (ExtensionSet::const_iterator it = service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
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
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_DidGetApplicationInfo,
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
    IPC_MESSAGE_HANDLER(ChromeViewHostMsg_DetailedConsoleMessageAdded,
                        OnDetailedConsoleMessageAdded)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TabHelper::CreateHostedApp() {
  // Add urls from the WebApplicationInfo.
  std::vector<GURL> web_app_info_icon_urls;
  for (std::vector<WebApplicationInfo::IconInfo>::const_iterator it =
           web_app_info_.icons.begin();
       it != web_app_info_.icons.end(); ++it) {
    if (it->url.is_valid())
      web_app_info_icon_urls.push_back(it->url);
  }

  favicon_downloader_.reset(
      new FaviconDownloader(web_contents(),
                            web_app_info_icon_urls,
                            base::Bind(&TabHelper::FinishCreateHostedApp,
                                       base::Unretained(this))));
  favicon_downloader_->Start();
}

// TODO(calamity): Move hosted app generation into its own file.
void TabHelper::FinishCreateHostedApp(
    bool success,
    const std::map<GURL, std::vector<SkBitmap> >& bitmaps) {
  // The tab has navigated away during the icon download. Cancel the hosted app
  // creation.
  if (!success) {
    favicon_downloader_.reset();
    return;
  }

  if (web_app_info_.app_url.is_empty())
    web_app_info_.app_url = web_contents()->GetURL();

  if (web_app_info_.title.empty())
    web_app_info_.title = web_contents()->GetTitle();
  if (web_app_info_.title.empty())
    web_app_info_.title = base::UTF8ToUTF16(web_app_info_.app_url.spec());

  // Add the downloaded icons. Extensions only allow certain icon sizes. First
  // populate icons that match the allowed sizes exactly and then downscale
  // remaining icons to the closest allowed size that doesn't yet have an icon.
  std::set<int> allowed_sizes(
      extension_misc::kExtensionIconSizes,
      extension_misc::kExtensionIconSizes +
          extension_misc::kNumExtensionIconSizes);
  std::vector<SkBitmap> downloaded_icons;
  for (FaviconDownloader::FaviconMap::const_iterator map_it = bitmaps.begin();
       map_it != bitmaps.end(); ++map_it) {
    for (std::vector<SkBitmap>::const_iterator bitmap_it =
             map_it->second.begin();
         bitmap_it != map_it->second.end(); ++bitmap_it) {
      if (bitmap_it->empty() || bitmap_it->width() != bitmap_it->height())
        continue;

      downloaded_icons.push_back(*bitmap_it);
    }
  }

  // If there are icons that don't match the accepted icon sizes, find the
  // closest bigger icon to the accepted sizes and resize the icon to it. An
  // icon will be resized and used for at most one size.
  std::map<int, SkBitmap> resized_bitmaps(
      TabHelper::ConstrainBitmapsToSizes(downloaded_icons,
                                         allowed_sizes));

  // Generate container icons from smaller icons.
  const int kIconSizesToGenerate[] = {
    extension_misc::EXTENSION_ICON_SMALL,
    extension_misc::EXTENSION_ICON_MEDIUM,
  };
  const std::set<int> generate_sizes(
      kIconSizesToGenerate,
      kIconSizesToGenerate + arraysize(kIconSizesToGenerate));

  // Only generate icons if larger icons don't exist. This means the app
  // launcher and the taskbar will do their best downsizing large icons and
  // these container icons are only generated as a last resort against upscaling
  // a smaller icon.
  if (resized_bitmaps.lower_bound(*generate_sizes.rbegin()) ==
      resized_bitmaps.end()) {
    // Generate these from biggest to smallest so we don't end up with
    // concentric container icons.
    for (std::set<int>::const_reverse_iterator it = generate_sizes.rbegin();
         it != generate_sizes.rend(); ++it) {
      TabHelper::GenerateContainerIcon(&resized_bitmaps, *it);
    }
  }

  // Populate a the icon data into the WebApplicationInfo we are using to
  // install the bookmark app.
  for (std::map<int, SkBitmap>::const_iterator resized_bitmaps_it =
           resized_bitmaps.begin();
       resized_bitmaps_it != resized_bitmaps.end(); ++resized_bitmaps_it) {
    WebApplicationInfo::IconInfo icon_info;
    icon_info.data = resized_bitmaps_it->second;
    icon_info.width = icon_info.data.width();
    icon_info.height = icon_info.data.height();
    web_app_info_.icons.push_back(icon_info);
  }

  // Install the app.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::CreateSilent(profile->GetExtensionService()));
  installer->set_error_on_unsupported_requirements(true);
  installer->InstallWebApp(web_app_info_);
  favicon_downloader_.reset();
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

void TabHelper::OnDidGetApplicationInfo(int32 page_id,
                                        const WebApplicationInfo& info) {
  // Android does not implement BrowserWindow.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  web_app_info_ = info;

  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry || (entry->GetPageID() != page_id))
    return;

  switch (pending_web_app_action_) {
    case CREATE_SHORTCUT: {
      chrome::ShowCreateWebAppShortcutsDialog(
          web_contents()->GetView()->GetTopLevelNativeWindow(),
          web_contents());
      break;
    }
    case CREATE_HOSTED_APP: {
      CreateHostedApp();
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

void TabHelper::OnInlineWebstoreInstall(
    int install_id,
    int return_route_id,
    const std::string& webstore_item_id,
    const GURL& requestor_url) {
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
    int32 on_page_id,
    const GURL& on_url) {
  FOR_EACH_OBSERVER(ScriptExecutionObserver, script_execution_observers_,
                    OnScriptsExecuted(web_contents(),
                                      executing_scripts_map,
                                      on_page_id,
                                      on_url));
}

void TabHelper::OnWatchedPageChange(
    const std::vector<std::string>& css_selectors) {
#if defined(ENABLE_EXTENSIONS)
  if (ExtensionSystem::Get(profile_)->extension_service() &&
      RulesRegistryService::Get(profile_)) {
    RulesRegistryService::Get(profile_)->content_rules_registry()->Apply(
        web_contents(), css_selectors);
  }
#endif  // defined(ENABLE_EXTENSIONS)
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

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  ExtensionService* extension_service = profile->GetExtensionService();
  if (!extension_service || !extension_service->is_ready())
    return NULL;

  const Extension* extension =
      extension_service->GetExtensionById(extension_app_id, false);
  return extension;
}

void TabHelper::UpdateExtensionAppIcon(const Extension* extension) {
  extension_app_icon_.reset();
  // Ensure previously enqueued callbacks are ignored.
  image_loader_ptr_factory_.InvalidateWeakPtrs();

  // Enqueue OnImageLoaded callback.
  if (extension) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());
    extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile);
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

void TabHelper::GetApplicationInfo(int32 page_id) {
  Send(new ExtensionMsg_GetApplicationInfo(routing_id(), page_id));
}

void TabHelper::Observe(int type,
                        const content::NotificationSource& source,
                        const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_STOP: {
      const NavigationController& controller =
          *content::Source<NavigationController>(source).ptr();
      DCHECK_EQ(controller.GetWebContents(), web_contents());

      if (pending_web_app_action_ == UPDATE_SHORTCUT) {
        // Schedule a shortcut update when web application info is available if
        // last committed entry is not NULL. Last committed entry could be NULL
        // when an interstitial page is injected (e.g. bad https certificate,
        // malware site etc). When this happens, we abort the shortcut update.
        NavigationEntry* entry = controller.GetLastCommittedEntry();
        if (entry)
          GetApplicationInfo(entry->GetPageID());
        else
          pending_web_app_action_ = NONE;
      }
      break;
    }
    case chrome::NOTIFICATION_CRX_INSTALLER_DONE: {
      if (pending_web_app_action_ != CREATE_HOSTED_APP)
        return;

      pending_web_app_action_ = NONE;

      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension ||
          AppLaunchInfo::GetLaunchWebURL(extension) != web_app_info_.app_url)
        return;

#if defined(OS_CHROMEOS)
      ChromeLauncherController::instance()->PinAppWithID(extension->id());
#endif

      // Android does not implement browser_finder.cc.
#if !defined(OS_ANDROID)
      Browser* browser =
          chrome::FindBrowserWithWebContents(web_contents());
      if (browser) {
        browser->window()->ShowBookmarkAppBubble(web_app_info_,
                                                 extension->id());
      }
#endif
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR: {
      if (pending_web_app_action_ == CREATE_HOSTED_APP)
        pending_web_app_action_ = NONE;
      break;
    }
  }
}

void TabHelper::SetTabId(RenderViewHost* render_view_host) {
  render_view_host->Send(
      new ExtensionMsg_SetTabId(render_view_host->GetRoutingID(),
                                SessionID::IdForTab(web_contents())));
}

}  // namespace extensions
