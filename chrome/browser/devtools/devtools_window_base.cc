// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chrome_page_zoom.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
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
#include "components/infobars/core/infobar.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using base::DictionaryValue;
using content::BrowserThread;

namespace {

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

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";

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

}  // namespace

// DevToolsWindowBase::FrontendWebContentsObserver ----------------------------

class DevToolsWindowBase::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(DevToolsWindowBase* window);
  virtual ~FrontendWebContentsObserver();

 private:
  // contents::WebContentsObserver:
  virtual void AboutToNavigateRenderView(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DocumentOnLoadCompletedInMainFrame(int32 page_id) OVERRIDE;
  virtual void WebContentsDestroyed(content::WebContents*) OVERRIDE;

  DevToolsWindowBase* devtools_window_;
  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
};

DevToolsWindowBase::FrontendWebContentsObserver::FrontendWebContentsObserver(
    DevToolsWindowBase* devtools_window)
    : WebContentsObserver(devtools_window->web_contents()),
      devtools_window_(devtools_window) {
}

void DevToolsWindowBase::FrontendWebContentsObserver::WebContentsDestroyed(
    content::WebContents* contents) {
  delete devtools_window_;
}

DevToolsWindowBase::FrontendWebContentsObserver::
    ~FrontendWebContentsObserver() {
}

void DevToolsWindowBase::FrontendWebContentsObserver::AboutToNavigateRenderView(
    content::RenderViewHost* render_view_host) {
  content::DevToolsClientHost::SetupDevToolsFrontendClient(render_view_host);
}

void DevToolsWindowBase::FrontendWebContentsObserver::
    DocumentOnLoadCompletedInMainFrame(int32 page_id) {
  devtools_window_->DocumentOnLoadCompletedInMainFrame();
}

// DevToolsWindowBase ---------------------------------------------------------

DevToolsWindowBase::~DevToolsWindowBase() {
  content::DevToolsManager::GetInstance()->ClientHostClosing(
      frontend_host_.get());

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
  if (device_listener_enabled_)
    EnableRemoteDeviceCounter(false);
}

void DevToolsWindowBase::InspectedContentsClosing() {
  web_contents_->GetRenderViewHost()->ClosePage();
}

DevToolsWindowBase::DevToolsWindowBase(content::WebContents* web_contents,
                                       const GURL& url)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      device_listener_enabled_(false),
      weak_factory_(this) {
  frontend_contents_observer_.reset(new FrontendWebContentsObserver(this));
  web_contents_->GetMutableRendererPrefs()->can_accept_load_drops = false;

  web_contents_->GetController().LoadURL(
      ApplyThemeToURL(url), content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  frontend_host_.reset(content::DevToolsClientHost::CreateDevToolsFrontendHost(
      web_contents_, this));
  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile_));
  file_system_indexer_ = new DevToolsFileSystemIndexer();
  extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
      web_contents_);

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

GURL DevToolsWindowBase::ApplyThemeToURL(const GURL& base_url) {
  std::string frontend_url = base_url.spec();
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
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
  return GURL(url_string);
}

void DevToolsWindowBase::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  UpdateTheme();
}

void DevToolsWindowBase::DispatchOnEmbedder(const std::string& message) {
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
    scoped_ptr<base::Value> id_value(base::Value::CreateIntegerValue(id));
    scoped_ptr<base::Value> error_value(base::Value::CreateStringValue(error));
    CallClientFunction("InspectorFrontendAPI.embedderMessageAck",
                       id_value.get(), error_value.get(), NULL);
  }
}

void DevToolsWindowBase::ActivateWindow() {
  web_contents_->GetDelegate()->ActivateContents(web_contents_);
  web_contents_->GetView()->Focus();
}

void DevToolsWindowBase::CloseWindow() {
}

void DevToolsWindowBase::SetContentsInsets(
    int top, int left, int bottom, int right) {
}

void DevToolsWindowBase::SetContentsResizingStrategy(
    const gfx::Insets& insets, const gfx::Size& min_size) {
}

void DevToolsWindowBase::MoveWindow(int x, int y) {
}

void DevToolsWindowBase::SetIsDocked(bool dock_requested) {
}

void DevToolsWindowBase::InspectElementCompleted() {
}

void DevToolsWindowBase::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  Browser* browser = FindBrowser(web_contents_);
  browser->OpenURL(params);
}

void DevToolsWindowBase::SaveToFile(const std::string& url,
                                    const std::string& content,
                                    bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsWindowBase::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url),
                     base::Bind(&DevToolsWindowBase::CanceledFileSaveAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindowBase::AppendToFile(const std::string& url,
                                      const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsWindowBase::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindowBase::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RequestFileSystems(base::Bind(
      &DevToolsWindowBase::FileSystemsLoaded, weak_factory_.GetWeakPtr()));
}

void DevToolsWindowBase::AddFileSystem() {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      base::Bind(&DevToolsWindowBase::FileSystemAdded,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsWindowBase::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsWindowBase::RemoveFileSystem(
    const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
}

void DevToolsWindowBase::UpgradeDraggedFileSystemPermissions(
    const std::string& file_system_url) {
  CHECK(web_contents_->GetURL().SchemeIs(content::kChromeDevToolsScheme));
  file_helper_->UpgradeDraggedFileSystemPermissions(
      file_system_url,
      base::Bind(&DevToolsWindowBase::FileSystemAdded,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsWindowBase::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsWindowBase::IndexPath(int request_id,
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
              Bind(&DevToolsWindowBase::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindowBase::IndexingWorked,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindowBase::IndexingDone,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path)));
}

void DevToolsWindowBase::StopIndexing(int request_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  IndexingJobsMap::iterator it = indexing_jobs_.find(request_id);
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsWindowBase::SearchInPath(int request_id,
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
                                     Bind(&DevToolsWindowBase::SearchCompleted,
                                          weak_factory_.GetWeakPtr(),
                                          request_id,
                                          file_system_path));
}

void DevToolsWindowBase::SetWhitelistedShortcuts(
    const std::string& message) {
}

void DevToolsWindowBase::ZoomIn() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_IN);
}

void DevToolsWindowBase::ZoomOut() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_OUT);
}

void DevToolsWindowBase::ResetZoom() {
  chrome_page_zoom::Zoom(web_contents(), content::PAGE_ZOOM_RESET);
}

void DevToolsWindowBase::OpenUrlOnRemoteDeviceAndInspect(
    const std::string& browser_id,
    const std::string& url) {
  if (remote_targets_handler_)
    remote_targets_handler_->OpenAndInspect(browser_id, url, profile_);
}

void DevToolsWindowBase::StartRemoteDevicesListener() {
  remote_targets_handler_ = DevToolsRemoteTargetsUIHandler::CreateForAdb(
      base::Bind(&DevToolsWindowBase::PopulateRemoteDevices,
                 base::Unretained(this)),
      profile_);
}

void DevToolsWindowBase::StopRemoteDevicesListener() {
  remote_targets_handler_.reset();
}

void DevToolsWindowBase::EnableRemoteDeviceCounter(bool enable) {
  DevToolsAndroidBridge* adb_bridge =
      DevToolsAndroidBridge::Factory::GetForProfile(profile_);
  if (!adb_bridge)
    return;

  DCHECK(device_listener_enabled_ != enable);
  device_listener_enabled_ = enable;
  if (enable)
    adb_bridge->AddDeviceCountListener(this);
  else
    adb_bridge->RemoveDeviceCountListener(this);
}

void DevToolsWindowBase::DeviceCountChanged(int count) {
  base::FundamentalValue value(count);
  CallClientFunction(
      "InspectorFrontendAPI.setRemoteDeviceCount", &value, NULL, NULL);
}

void DevToolsWindowBase::PopulateRemoteDevices(
    const std::string& source,
    scoped_ptr<base::ListValue> targets) {
  CallClientFunction(
      "InspectorFrontendAPI.populateRemoteDevices", targets.get(), NULL, NULL);
}

void DevToolsWindowBase::FileSavedAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.savedURL", &url_value, NULL, NULL);
}

void DevToolsWindowBase::CanceledFileSaveAs(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.canceledSaveURL",
                     &url_value, NULL, NULL);
}

void DevToolsWindowBase::AppendedTo(const std::string& url) {
  base::StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsWindowBase::FileSystemsLoaded(
    const std::vector<DevToolsFileHelper::FileSystem>& file_systems) {
  base::ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("InspectorFrontendAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsWindowBase::FileSystemAdded(
    const DevToolsFileHelper::FileSystem& file_system) {
  scoped_ptr<base::StringValue> error_string_value(
      new base::StringValue(std::string()));
  scoped_ptr<base::DictionaryValue> file_system_value;
  if (!file_system.file_system_path.empty())
    file_system_value.reset(CreateFileSystemValue(file_system));
  CallClientFunction("InspectorFrontendAPI.fileSystemAdded",
                     error_string_value.get(), file_system_value.get(), NULL);
}

void DevToolsWindowBase::IndexingTotalWorkCalculated(
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

void DevToolsWindowBase::IndexingWorked(int request_id,
                                        const std::string& file_system_path,
                                        int worked) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  base::FundamentalValue worked_value(worked);
  CallClientFunction("InspectorFrontendAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsWindowBase::IndexingDone(int request_id,
                                      const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  base::StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsWindowBase::SearchCompleted(
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

void DevToolsWindowBase::ShowDevToolsConfirmInfoBar(
    const base::string16& message,
    const InfoBarCallback& callback) {
  DevToolsConfirmInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents_),
      callback, message);
}

void DevToolsWindowBase::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  std::string command("InspectorFrontendAPI.setToolbarColors(\"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "\", \"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)) +
      "\")");
  web_contents_->GetMainFrame()->ExecuteJavaScript(base::ASCIIToUTF16(command));
}

void DevToolsWindowBase::AddDevToolsExtensionsToClient() {
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
    extension_info->Set(
        "exposeExperimentalAPIs",
        new base::FundamentalValue((*extension)->HasAPIPermission(
            extensions::APIPermission::kExperimental)));
    results.Append(extension_info);
  }
  CallClientFunction("WebInspector.addExtensions", &results, NULL, NULL);
}

void DevToolsWindowBase::CallClientFunction(const std::string& function_name,
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

void DevToolsWindowBase::DocumentOnLoadCompletedInMainFrame() {
  UpdateTheme();
  AddDevToolsExtensionsToClient();
}
