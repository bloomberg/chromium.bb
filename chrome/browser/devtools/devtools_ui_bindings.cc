// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_ui_bindings.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/devtools/devtools_target_impl.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::DictionaryValue;
using content::BrowserThread;

namespace {

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";
static const char kTitleFormat[] = "Developer Tools - %s";

std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::IntToString(SkColorGetR(color)) + "," +
      base::IntToString(SkColorGetG(color)) + "," +
      base::IntToString(SkColorGetB(color)) + "," +
      base::DoubleToString(SkColorGetA(color) / 255.0) + ")";
}

base::DictionaryValue* CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  base::DictionaryValue* file_system_value = new base::DictionaryValue();
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

Browser* FindBrowser(content::WebContents* web_contents) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    int tab_index = it->tab_strip_model()->GetIndexOfWebContents(
        web_contents);
    if (tab_index != TabStripModel::kNoTab)
      return *it;
  }
  return NULL;
}

// DevToolsConfirmInfoBarDelegate ---------------------------------------------

typedef base::Callback<void(bool)> InfoBarCallback;

class DevToolsConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If |infobar_service| is NULL, runs |callback| with a single argument with
  // value "false".  Otherwise, creates a dev tools confirm infobar and delegate
  // and adds the infobar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const InfoBarCallback& callback,
                     const base::string16& message);

 private:
  DevToolsConfirmInfoBarDelegate(
      const InfoBarCallback& callback,
      const base::string16& message);
  virtual ~DevToolsConfirmInfoBarDelegate();

  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  InfoBarCallback callback_;
  const base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsConfirmInfoBarDelegate);
};

void DevToolsConfirmInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const InfoBarCallback& callback,
    const base::string16& message) {
  if (!infobar_service) {
    callback.Run(false);
    return;
  }

  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new DevToolsConfirmInfoBarDelegate(callback, message))));
}

DevToolsConfirmInfoBarDelegate::DevToolsConfirmInfoBarDelegate(
    const InfoBarCallback& callback,
    const base::string16& message)
    : ConfirmInfoBarDelegate(),
      callback_(callback),
      message_(message) {
}

DevToolsConfirmInfoBarDelegate::~DevToolsConfirmInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

base::string16 DevToolsConfirmInfoBarDelegate::GetMessageText() const {
  return message_;
}

base::string16 DevToolsConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_DEV_TOOLS_CONFIRM_ALLOW_BUTTON : IDS_DEV_TOOLS_CONFIRM_DENY_BUTTON);
}

bool DevToolsConfirmInfoBarDelegate::Accept() {
  callback_.Run(true);
  callback_.Reset();
  return true;
}

bool DevToolsConfirmInfoBarDelegate::Cancel() {
  callback_.Run(false);
  callback_.Reset();
  return true;
}

// DevToolsUIDefaultDelegate --------------------------------------------------

class DefaultBindingsDelegate : public DevToolsUIBindings::Delegate {
 public:
  explicit DefaultBindingsDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

 private:
  virtual ~DefaultBindingsDelegate() {}

  virtual void ActivateWindow() OVERRIDE;
  virtual void CloseWindow() OVERRIDE {}
  virtual void SetInspectedPageBounds(const gfx::Rect& rect) OVERRIDE {}
  virtual void SetContentsResizingStrategy(
      const gfx::Insets& insets, const gfx::Size& min_size) OVERRIDE {}
  virtual void InspectElementCompleted() OVERRIDE {}
  virtual void MoveWindow(int x, int y) OVERRIDE {}
  virtual void SetIsDocked(bool is_docked) OVERRIDE {}
  virtual void OpenInNewTab(const std::string& url) OVERRIDE;
  virtual void SetWhitelistedShortcuts(const std::string& message) OVERRIDE {}

  virtual void InspectedContentsClosing() OVERRIDE;
  virtual void OnLoadCompleted() OVERRIDE {}
  virtual InfoBarService* GetInfoBarService() OVERRIDE;
  virtual void RenderProcessGone() OVERRIDE {}

  content::WebContents* web_contents_;
  DISALLOW_COPY_AND_ASSIGN(DefaultBindingsDelegate);
};

void DefaultBindingsDelegate::ActivateWindow() {
  web_contents_->GetDelegate()->ActivateContents(web_contents_);
  web_contents_->Focus();
}

void DefaultBindingsDelegate::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  Browser* browser = FindBrowser(web_contents_);
  browser->OpenURL(params);
}

void DefaultBindingsDelegate::InspectedContentsClosing() {
  web_contents_->GetRenderViewHost()->ClosePage();
}

InfoBarService* DefaultBindingsDelegate::GetInfoBarService() {
  return InfoBarService::FromWebContents(web_contents_);
}

}  // namespace

// DevToolsUIBindings::FrontendWebContentsObserver ----------------------------

class DevToolsUIBindings::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsUIBindings* ui_bindings);
  virtual ~FrontendWebContentsObserver();

 private:
  // contents::WebContentsObserver:
  virtual void WebContentsDestroyed() OVERRIDE;
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE;
  virtual void AboutToNavigateRenderView(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame() OVERRIDE;

  DevToolsUIBindings* devtools_bindings_;
  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
};

DevToolsUIBindings::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsUIBindings* devtools_ui_bindings)
    : WebContentsObserver(devtools_ui_bindings->web_contents()),
      devtools_bindings_(devtools_ui_bindings) {
}

DevToolsUIBindings::FrontendWebContentsObserver::
    ~FrontendWebContentsObserver() {
}

void DevToolsUIBindings::FrontendWebContentsObserver::WebContentsDestroyed() {
  delete devtools_bindings_;
}

void DevToolsUIBindings::FrontendWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      content::DevToolsManager::GetInstance()->ClientHostClosing(
          devtools_bindings_);
      break;
    default:
      break;
  }
  devtools_bindings_->delegate_->RenderProcessGone();
}

void DevToolsUIBindings::FrontendWebContentsObserver::AboutToNavigateRenderView(
    content::RenderViewHost* render_view_host) {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  if (devtools_bindings_->url_ == entry->GetURL()) {
    devtools_bindings_->frontend_host_.reset(
        content::DevToolsFrontendHost::Create(
            render_view_host, devtools_bindings_));
  } else {
    delete devtools_bindings_;
  }
}

void DevToolsUIBindings::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInMainFrame() {
  devtools_bindings_->DocumentOnLoadCompletedInMainFrame();
}

// DevToolsUIBindings ---------------------------------------------------------

// static
GURL DevToolsUIBindings::ApplyThemeToURL(Profile* profile,
                                         const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile);
  DCHECK(tp);
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
      "dockSide=undocked" + // TODO(dgozman): remove this support in M38.
      "&toolbarColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "&textColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDevToolsExperiments))
    url_string += "&experiments=true";
#if defined(DEBUG_DEVTOOLS)
  url_string += "&debugFrontend=true";
#endif  // defined(DEBUG_DEVTOOLS)
  return GURL(url_string);
}

DevToolsUIBindings::DevToolsUIBindings(content::WebContents* web_contents,
                                       const GURL& url)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      delegate_(new DefaultBindingsDelegate(web_contents_)),
      device_count_updates_enabled_(false),
      devices_updates_enabled_(false),
      url_(url),
      weak_factory_(this) {
  frontend_contents_observer_.reset(new FrontendWebContentsObserver(this));
  web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;

  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile_));
  file_system_indexer_ = new DevToolsFileSystemIndexer();
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);

  web_contents_->GetController().LoadURL(
      url, content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  // Wipe out page icon so that the default application icon is used.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  entry->GetFavicon().image = gfx::Image();
  entry->GetFavicon().valid = true;

  // Register on-load actions.
  registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));

  embedder_message_dispatcher_.reset(
      DevToolsEmbedderMessageDispatcher::createForDevToolsFrontend(this));
}

DevToolsUIBindings::~DevToolsUIBindings() {
  content::DevToolsManager::GetInstance()->ClientHostClosing(this);

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
  SetDeviceCountUpdatesEnabled(false);
  SetDevicesUpdatesEnabled(false);
}

// content::NotificationObserver overrides ------------------------------------
void DevToolsUIBindings::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  UpdateTheme();
}

// content::DevToolsFrontendHost::Delegate implementation ---------------------
void DevToolsUIBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::ListValue empty_params;
  base::ListValue* params = &empty_params;

  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString(kFrontendHostMethod, &method) ||
      (dict->HasKey(kFrontendHostParams) &&
          !dict->GetList(kFrontendHostParams, &params))) {
    LOG(ERROR) << "Invalid message was sent to embedder: " << message;
    return;
  }

  int id = 0;
  dict->GetInteger(kFrontendHostId, &id);

  std::string error;
  embedder_message_dispatcher_->Dispatch(method, params, &error);
  if (id) {
    base::FundamentalValue id_value(id);
    base::StringValue error_value(error);
    CallClientFunction("InspectorFrontendAPI.embedderMessageAck",
                       &id_value, &error_value, NULL);
  }
}

void DevToolsUIBindings::HandleMessageFromDevToolsFrontendToBackend(
    const std::string& message) {
  content::DevToolsManager::GetInstance()->DispatchOnInspectorBackend(
      this, message);
}

// content::DevToolsClientHost implementation ---------------------------------
void DevToolsUIBindings::DispatchOnInspectorFrontend(
    const std::string& message) {
  if (frontend_host_)
    frontend_host_->DispatchOnDevToolsFrontend(message);
}

void DevToolsUIBindings::InspectedContentsClosing() {
  delegate_->InspectedContentsClosing();
}

void DevToolsUIBindings::ReplacedWithAnotherClient() {
}

// DevToolsEmbedderMessageDispatcher::Delegate implementation -----------------
void DevToolsUIBindings::ActivateWindow() {
  delegate_->ActivateWindow();
}

void DevToolsUIBindings::CloseWindow() {
  delegate_->CloseWindow();
}

void DevToolsUIBindings::SetInspectedPageBounds(const gfx::Rect& rect) {
  delegate_->SetInspectedPageBounds(rect);
}

void DevToolsUIBindings::SetContentsResizingStrategy(
    const gfx::Insets& insets, const gfx::Size& min_size) {
  delegate_->SetContentsResizingStrategy(insets, min_size);
}

void DevToolsUIBindings::MoveWindow(int x, int y) {
  delegate_->MoveWindow(x, y);
}

void DevToolsUIBindings::SetIsDocked(bool dock_requested) {
  delegate_->SetIsDocked(dock_requested);
}

void DevToolsUIBindings::InspectElementCompleted() {
  delegate_->InspectElementCompleted();
}

void DevToolsUIBindings::InspectedURLChanged(const std::string& url) {
  content::NavigationController& controller = web_contents()->GetController();
  content::NavigationEntry* entry = controller.GetActiveEntry();
  // DevTools UI is not localized.
  entry->SetTitle(
      base::UTF8ToUTF16(base::StringPrintf(kTitleFormat, url.c_str())));
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_TITLE);
}

void DevToolsUIBindings::OpenInNewTab(const std::string& url) {
  delegate_->OpenInNewTab(url);
}

void DevToolsUIBindings::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsUIBindings::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url),
                     base::Bind(&DevToolsUIBindings::CanceledFileSaveAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsUIBindings::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsUIBindings::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RequestFileSystems(base::Bind(
      &DevToolsUIBindings::FileSystemsLoaded, weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::AddFileSystem() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      base::Bind(&DevToolsUIBindings::FileSystemAdded,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsUIBindings::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::RemoveFileSystem(
    const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
}

void DevToolsUIBindings::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::Bind(&DevToolsUIBindings::FileSystemAdded,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsUIBindings::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsUIBindings::IndexPath(int request_id,
                                   const std::string& file_system_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(request_id, file_system_path);
    return;
  }
  indexing_jobs_[request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path,
              Bind(&DevToolsUIBindings::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingWorked,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsUIBindings::IndexingDone,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path)));
}

void DevToolsUIBindings::StopIndexing(int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IndexingJobsMap::iterator it = indexing_jobs_.find(request_id);
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsUIBindings::SearchInPath(int request_id,
                                      const std::string& file_system_path,
                                      const std::string& query) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(request_id, file_system_path, std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     Bind(&DevToolsUIBindings::SearchCompleted,
                                          weak_factory_.GetWeakPtr(),
                                          request_id,
                                          file_system_path));
}

void DevToolsUIBindings::SetWhitelistedShortcuts(
    const std::string& message) {
  delegate_->SetWhitelistedShortcuts(message);
}

void DevToolsUIBindings::ZoomIn() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
}

void DevToolsUIBindings::ZoomOut() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
}

void DevToolsUIBindings::ResetZoom() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
}

static void InspectTarget(Profile* profile, DevToolsTargetImpl* target) {
  if (target)
    target->Inspect(profile);
}

void DevToolsUIBindings::OpenUrlOnRemoteDeviceAndInspect(
    const std::string& browser_id,
    const std::string& url) {
  if (remote_targets_handler_) {
    remote_targets_handler_->Open(browser_id, url,
        base::Bind(&InspectTarget, profile_));
  }
}

void DevToolsUIBindings::SetDeviceCountUpdatesEnabled(bool enabled) {
  if (device_count_updates_enabled_ == enabled)
    return;
  DevToolsAndroidBridge* adb_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (!adb_bridge)
    return;

  device_count_updates_enabled_ = enabled;
  if (enabled)
    adb_bridge->AddDeviceCountListener(this);
  else
    adb_bridge->RemoveDeviceCountListener(this);
}

void DevToolsUIBindings::SetDevicesUpdatesEnabled(bool enabled) {
  if (devices_updates_enabled_ == enabled)
    return;
  devices_updates_enabled_ = enabled;
  if (enabled) {
    remote_targets_handler_ = DevToolsTargetsUIHandler::CreateForAdb(
        base::Bind(&DevToolsUIBindings::DevicesUpdated,
                   base::Unretained(this)),
        profile_);
  } else {
    remote_targets_handler_.reset();
  }
}

void DevToolsUIBindings::DeviceCountChanged(int count) {
  base::FundamentalValue value(count);
  CallClientFunction("InspectorFrontendAPI.deviceCountUpdated", &value, NULL,
                     NULL);
}

void DevToolsUIBindings::DevicesUpdated(
    const std::string& source,
    const base::ListValue& targets) {
  CallClientFunction("InspectorFrontendAPI.devicesUpdated", &targets, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSavedAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.savedURL", &url_value, NULL, NULL);
}

void DevToolsUIBindings::CanceledFileSaveAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.canceledSaveURL",
                     &url_value, NULL, NULL);
}

void DevToolsUIBindings::AppendedTo(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsUIBindings::FileSystemsLoaded(
    const std::vector<DevToolsFileHelper::FileSystem>& file_systems) {
  base::ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("InspectorFrontendAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsUIBindings::FileSystemAdded(
    const DevToolsFileHelper::FileSystem& file_system) {
  scoped_ptr<base::StringValue> error_string_value(
      new base::StringValue(std::string()));
  scoped_ptr<base::DictionaryValue> file_system_value;
  if (!file_system.file_system_path.empty())
    file_system_value.reset(CreateFileSystemValue(file_system));
  CallClientFunction("InspectorFrontendAPI.fileSystemAdded",
                     error_string_value.get(), file_system_value.get(), NULL);
}

void DevToolsUIBindings::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue total_work_value(total_work);
  CallClientFunction("InspectorFrontendAPI.indexingTotalWorkCalculated",
                     &request_id_value, &file_system_path_value,
                     &total_work_value);
}

void DevToolsUIBindings::IndexingWorked(int request_id,
                                        const std::string& file_system_path,
                                        int worked) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue worked_value(worked);
  CallClientFunction("InspectorFrontendAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsUIBindings::IndexingDone(int request_id,
                                      const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsUIBindings::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::ListValue file_paths_value;
  for (std::vector<std::string>::const_iterator it(file_paths.begin());
       it != file_paths.end(); ++it) {
    file_paths_value.AppendString(*it);
  }
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.searchCompleted", &request_id_value,
                     &file_system_path_value, &file_paths_value);
}

void DevToolsUIBindings::ShowDevToolsConfirmInfoBar(
    const base::string16& message,
    const InfoBarCallback& callback) {
  DevToolsConfirmInfoBarDelegate::Create(delegate_->GetInfoBarService(),
      callback, message);
}

void DevToolsUIBindings::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  std::string command("InspectorFrontendAPI.setToolbarColors(\"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "\", \"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)) +
      "\")");
  web_contents_->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(command));
}

void DevToolsUIBindings::AddDevToolsExtensionsToClient() {
  const ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      profile_->GetOriginalProfile())->extension_service();
  if (!extension_service)
    return;
  const extensions::ExtensionSet* extensions = extension_service->extensions();

  base::ListValue results;
  for (extensions::ExtensionSet::const_iterator extension(extensions->begin());
       extension != extensions->end(); ++extension) {
    if (extensions::ManifestURL::GetDevToolsPage(extension->get()).is_empty())
      continue;
    base::DictionaryValue* extension_info = new base::DictionaryValue();
    extension_info->Set(
        "startPage",
        new base::StringValue(
            extensions::ManifestURL::GetDevToolsPage(
                extension->get()).spec()));
    extension_info->Set("name", new base::StringValue((*extension)->name()));
    extension_info->Set("exposeExperimentalAPIs",
                        new base::FundamentalValue(
                            (*extension)->permissions_data()->HasAPIPermission(
                                extensions::APIPermission::kExperimental)));
    results.Append(extension_info);
  }
  CallClientFunction("WebInspector.addExtensions", &results, NULL, NULL);
}

void DevToolsUIBindings::SetDelegate(Delegate* delegate) {
  delegate_.reset(delegate);
}

void DevToolsUIBindings::CallClientFunction(const std::string& function_name,
                                            const base::Value* arg1,
                                            const base::Value* arg2,
                                            const base::Value* arg3) {
  std::string params;
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(arg1, &json);
    params.append(json);
    if (arg2) {
      base::JSONWriter::Write(arg2, &json);
      params.append(", " + json);
      if (arg3) {
        base::JSONWriter::Write(arg3, &json);
        params.append(", " + json);
      }
    }
  }
  base::string16 javascript =
      base::UTF8ToUTF16(function_name + "(" + params + ");");
  web_contents_->GetMainFrame()->ExecuteJavaScript(javascript);
}

void DevToolsUIBindings::DocumentOnLoadCompletedInMainFrame() {
  // Call delegate first - it seeds importants bit of information.
  delegate_->OnLoadCompleted();

  UpdateTheme();
  AddDevToolsExtensionsToClient();
}
