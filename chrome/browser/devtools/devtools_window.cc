// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/devtools/devtools_window.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/devtools_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/load_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::DevToolsAgentHost;


// DevToolsConfirmInfoBarDelegate ---------------------------------------------

class DevToolsConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // If |infobar_service| is NULL, runs |callback| with a single argument with
  // value "false".  Otherwise, creates a dev tools confirm infobar and delegate
  // and adds the inofbar to |infobar_service|.
  static void Create(InfoBarService* infobar_service,
                     const DevToolsWindow::InfoBarCallback& callback,
                     const string16& message);

 private:
  DevToolsConfirmInfoBarDelegate(
      InfoBarService* infobar_service,
      const DevToolsWindow::InfoBarCallback& callback,
      const string16& message);
  virtual ~DevToolsConfirmInfoBarDelegate();

  virtual string16 GetMessageText() const OVERRIDE;
  virtual string16 GetButtonLabel(InfoBarButton button) const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  DevToolsWindow::InfoBarCallback callback_;
  const string16 message_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsConfirmInfoBarDelegate);
};

void DevToolsConfirmInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const DevToolsWindow::InfoBarCallback& callback,
    const string16& message) {
  if (!infobar_service) {
    callback.Run(false);
    return;
  }

  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new DevToolsConfirmInfoBarDelegate(infobar_service, callback, message)));
}

DevToolsConfirmInfoBarDelegate::DevToolsConfirmInfoBarDelegate(
    InfoBarService* infobar_service,
    const DevToolsWindow::InfoBarCallback& callback,
    const string16& message)
    : ConfirmInfoBarDelegate(infobar_service),
      callback_(callback),
      message_(message) {
}

DevToolsConfirmInfoBarDelegate::~DevToolsConfirmInfoBarDelegate() {
  if (!callback_.is_null())
    callback_.Run(false);
}

string16 DevToolsConfirmInfoBarDelegate::GetMessageText() const {
  return message_;
}

string16 DevToolsConfirmInfoBarDelegate::GetButtonLabel(
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


// DevToolsWindow::InspectedWebContentsObserver -------------------------------

class DevToolsWindow::InspectedWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit InspectedWebContentsObserver(content::WebContents* web_contents);
  virtual ~InspectedWebContentsObserver();

  content::WebContents* web_contents() {
    return WebContentsObserver::web_contents();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InspectedWebContentsObserver);
};

DevToolsWindow::InspectedWebContentsObserver::InspectedWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::InspectedWebContentsObserver::~InspectedWebContentsObserver() {
}


// DevToolsWindow::FrontendWebContentsObserver --------------------------------

class DevToolsWindow::FrontendWebContentsObserver
    : public content::WebContentsObserver {
 public:
  explicit FrontendWebContentsObserver(content::WebContents* web_contents);
  virtual ~FrontendWebContentsObserver();

 private:
  // contents::WebContentsObserver:
  virtual void AboutToNavigateRenderView(
      content::RenderViewHost* render_view_host) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FrontendWebContentsObserver);
};

DevToolsWindow::FrontendWebContentsObserver::FrontendWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
}

DevToolsWindow::FrontendWebContentsObserver::~FrontendWebContentsObserver() {
}

void DevToolsWindow::FrontendWebContentsObserver::AboutToNavigateRenderView(
    content::RenderViewHost* render_view_host) {
  content::DevToolsClientHost::SetupDevToolsFrontendClient(render_view_host);
}


// DevToolsWindow -------------------------------------------------------------

namespace {

typedef std::vector<DevToolsWindow*> DevToolsWindows;
base::LazyInstance<DevToolsWindows>::Leaky g_instances =
    LAZY_INSTANCE_INITIALIZER;

const char kPrefBottom[] = "dock_bottom";
const char kPrefRight[] = "dock_right";
const char kPrefUndocked[] = "undocked";

const char kDockSideBottom[] = "bottom";
const char kDockSideRight[] = "right";
const char kDockSideUndocked[] = "undocked";
const char kDockSideMinimized[] = "minimized";

static const char kFrontendHostId[] = "id";
static const char kFrontendHostMethod[] = "method";
static const char kFrontendHostParams[] = "params";

const int kMinContentsSize = 50;

std::string SkColorToRGBAString(SkColor color) {
  // We avoid StringPrintf because it will use locale specific formatters for
  // the double (e.g. ',' instead of '.' in German).
  return "rgba(" + base::IntToString(SkColorGetR(color)) + "," +
      base::IntToString(SkColorGetG(color)) + "," +
      base::IntToString(SkColorGetB(color)) + "," +
      base::DoubleToString(SkColorGetA(color) / 255.0) + ")";
}

DictionaryValue* CreateFileSystemValue(
    DevToolsFileHelper::FileSystem file_system) {
  DictionaryValue* file_system_value = new DictionaryValue();
  file_system_value->SetString("fileSystemName", file_system.file_system_name);
  file_system_value->SetString("rootURL", file_system.root_url);
  file_system_value->SetString("fileSystemPath", file_system.file_system_path);
  return file_system_value;
}

}  // namespace

const char DevToolsWindow::kDevToolsApp[] = "DevToolsApp";

DevToolsWindow::~DevToolsWindow() {
  DevToolsWindows* instances = &g_instances.Get();
  DevToolsWindows::iterator it(
      std::find(instances->begin(), instances->end(), this));
  DCHECK(it != instances->end());
  instances->erase(it);

  for (IndexingJobsMap::const_iterator jobs_it(indexing_jobs_.begin());
       jobs_it != indexing_jobs_.end(); ++jobs_it) {
    jobs_it->second->Stop();
  }
  indexing_jobs_.clear();
}

// static
std::string DevToolsWindow::GetDevToolsWindowPlacementPrefKey() {
  return std::string(prefs::kBrowserWindowPlacement) + "_" +
      std::string(kDevToolsApp);
}

// static
void DevToolsWindow::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kDevToolsOpenDocked, true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDevToolsDockSide, kDockSideBottom,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsEditedFiles,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsFileSystemPaths,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDevToolsAdbKey, std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterDictionaryPref(
      GetDevToolsWindowPlacementPrefKey().c_str(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterBooleanPref(
      prefs::kDevToolsDiscoverUsbDevicesEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDevToolsPortForwardingEnabled,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDevToolsPortForwardingDefaultSet,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      prefs::kDevToolsPortForwardingConfig,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
DevToolsWindow* DevToolsWindow::GetDockedInstanceForInspectedTab(
    content::WebContents* inspected_web_contents) {
  if (!inspected_web_contents ||
      !DevToolsAgentHost::HasFor(inspected_web_contents->GetRenderViewHost()))
    return NULL;
  scoped_refptr<DevToolsAgentHost> agent(DevToolsAgentHost::GetOrCreateFor(
      inspected_web_contents->GetRenderViewHost()));
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  return (window && window->IsDocked()) ? window : NULL;
}

// static
bool DevToolsWindow::IsDevToolsWindow(content::RenderViewHost* window_rvh) {
  return AsDevToolsWindow(window_rvh) != NULL;
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindowForWorker(
    Profile* profile,
    DevToolsAgentHost* worker_agent) {
  DevToolsWindow* window = FindDevToolsWindow(worker_agent);
  if (!window) {
    window = DevToolsWindow::CreateDevToolsWindowForWorker(profile);
    // Will disconnect the current client host if there is one.
    content::DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        worker_agent, window->frontend_host_.get());
  }
  window->Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
  return window;
}

// static
DevToolsWindow* DevToolsWindow::CreateDevToolsWindowForWorker(
    Profile* profile) {
  return Create(profile, GURL(), NULL, DEVTOOLS_DOCK_SIDE_UNDOCKED, true,
                false);
}

// static
DevToolsWindow* DevToolsWindow::OpenDevToolsWindow(
    content::RenderViewHost* inspected_rvh) {
  return ToggleDevToolsWindow(inspected_rvh, true,
                              DEVTOOLS_TOGGLE_ACTION_SHOW);
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    Browser* browser,
    DevToolsToggleAction action) {
  if (action == DEVTOOLS_TOGGLE_ACTION_TOGGLE && browser->is_devtools()) {
    browser->tab_strip_model()->CloseAllTabs();
    return NULL;
  }

  return ToggleDevToolsWindow(
      browser->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost(),
      action == DEVTOOLS_TOGGLE_ACTION_INSPECT, action);
}

// static
void DevToolsWindow::OpenExternalFrontend(
    Profile* profile,
    const std::string& frontend_url,
    content::DevToolsAgentHost* agent_host) {
  DevToolsWindow* window = FindDevToolsWindow(agent_host);
  if (!window) {
    window = Create(profile, DevToolsUI::GetProxyURL(frontend_url), NULL,
                    DEVTOOLS_DOCK_SIDE_UNDOCKED, false, true);
    content::DevToolsManager::GetInstance()->RegisterDevToolsClientHostFor(
        agent_host, window->frontend_host_.get());
  }
  window->Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
}

// static
DevToolsWindow* DevToolsWindow::ToggleDevToolsWindow(
    content::RenderViewHost* inspected_rvh,
    bool force_open,
    DevToolsToggleAction action) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  DevToolsWindow* window = FindDevToolsWindow(agent.get());
  bool do_open = force_open;
  if (!window) {
    Profile* profile = Profile::FromBrowserContext(
        inspected_rvh->GetProcess()->GetBrowserContext());
    DevToolsDockSide dock_side = GetDockSideFromPrefs(profile);
    window = Create(profile, GURL(), inspected_rvh, dock_side, false, false);
    manager->RegisterDevToolsClientHostFor(agent.get(),
                                           window->frontend_host_.get());
    do_open = true;
  }

  // Update toolbar to reflect DevTools changes.
  window->UpdateBrowserToolbar();

  // If window is docked and visible, we hide it on toggle. If window is
  // undocked, we show (activate) it. If window is minimized, we maximize it.
  if (window->dock_side_ == DEVTOOLS_DOCK_SIDE_MINIMIZED)
    window->Restore();
  else if (!window->IsDocked() || do_open)
    window->Show(action);
  else
    window->CloseWindow();

  return window;
}

// static
void DevToolsWindow::InspectElement(content::RenderViewHost* inspected_rvh,
                                    int x,
                                    int y) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_rvh));
  agent->InspectElement(x, y);
  // TODO(loislo): we should initiate DevTools window opening from within
  // renderer. Otherwise, we still can hit a race condition here.
  OpenDevToolsWindow(inspected_rvh);
}

// static
int DevToolsWindow::GetMinimumWidth() {
  const int kMinDevToolsWidth = 150;
  return kMinDevToolsWidth;
}

// static
int DevToolsWindow::GetMinimumHeight() {
  // Minimal height of devtools pane or content pane when devtools are docked
  // to the browser window.
  const int kMinDevToolsHeight = 50;
  return kMinDevToolsHeight;
}

// static
int DevToolsWindow::GetMinimizedHeight() {
  const int kMinimizedDevToolsHeight = 24;
  return kMinimizedDevToolsHeight;
}

void DevToolsWindow::InspectedContentsClosing() {
  web_contents_->GetRenderViewHost()->ClosePage();
}

content::RenderViewHost* DevToolsWindow::GetRenderViewHost() {
  return web_contents_->GetRenderViewHost();
}

content::DevToolsClientHost* DevToolsWindow::GetDevToolsClientHostForTest() {
  return frontend_host_.get();
}

int DevToolsWindow::GetWidth(int container_width) {
  if (width_ == -1) {
    width_ = profile_->GetPrefs()->
        GetInteger(prefs::kDevToolsVSplitLocation);
  }

  // By default, size devtools as 1/3 of the browser window.
  if (width_ == -1)
    width_ = container_width / 3;

  // Respect the minimum devtools width preset.
  width_ = std::max(GetMinimumWidth(), width_);

  // But it should never compromise the content window size unless the entire
  // window is tiny.
  width_ = std::min(container_width - kMinContentsSize, width_);
  return width_;
}

int DevToolsWindow::GetHeight(int container_height) {
  if (height_ == -1) {
    height_ = profile_->GetPrefs()->
        GetInteger(prefs::kDevToolsHSplitLocation);
  }

  // By default, size devtools as 1/3 of the browser window.
  if (height_ == -1)
    height_ = container_height / 3;

  // Respect the minimum devtools width preset.
  height_ = std::max(GetMinimumHeight(), height_);

  // But it should never compromise the content window size.
  height_ = std::min(container_height - kMinContentsSize, height_);
  return height_;
}

void DevToolsWindow::SetWidth(int width) {
  width_ = width;
  profile_->GetPrefs()->SetInteger(prefs::kDevToolsVSplitLocation, width);
}

void DevToolsWindow::SetHeight(int height) {
  height_ = height;
  profile_->GetPrefs()->SetInteger(prefs::kDevToolsHSplitLocation, height);
}

void DevToolsWindow::Show(DevToolsToggleAction action) {
  if (IsDocked()) {
    Browser* inspected_browser = NULL;
    int inspected_tab_index = -1;
    // Tell inspected browser to update splitter and switch to inspected panel.
    if (!IsInspectedBrowserPopup() &&
        FindInspectedBrowserAndTabIndex(&inspected_browser,
                                        &inspected_tab_index)) {
      BrowserWindow* inspected_window = inspected_browser->window();
      web_contents_->SetDelegate(this);
      inspected_window->UpdateDevTools();
      web_contents_->GetView()->SetInitialFocus();
      inspected_window->Show();
      TabStripModel* tab_strip_model = inspected_browser->tab_strip_model();
      tab_strip_model->ActivateTabAt(inspected_tab_index, true);
      PrefsTabHelper::CreateForWebContents(web_contents_);
      GetRenderViewHost()->SyncRendererPrefs();
      ScheduleAction(action);
      return;
    }

    // Sometimes we don't know where to dock. Stay undocked.
    dock_side_ = DEVTOOLS_DOCK_SIDE_UNDOCKED;
  }

  // Avoid consecutive window switching if the devtools window has been opened
  // and the Inspect Element shortcut is pressed in the inspected tab.
  bool should_show_window =
      !browser_ || (action != DEVTOOLS_TOGGLE_ACTION_INSPECT);

  if (!browser_)
    CreateDevToolsBrowser();

  if (should_show_window) {
    browser_->window()->Show();
    web_contents_->GetView()->SetInitialFocus();
  }

  ScheduleAction(action);
}

DevToolsWindow::DevToolsWindow(Profile* profile,
                               const GURL& url,
                               content::RenderViewHost* inspected_rvh,
                               DevToolsDockSide dock_side)
    : profile_(profile),
      browser_(NULL),
      dock_side_(dock_side),
      is_loaded_(false),
      action_on_load_(DEVTOOLS_TOGGLE_ACTION_SHOW),
      width_(-1),
      height_(-1),
      dock_side_before_minimized_(dock_side),
      weak_factory_(this) {
  web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  frontend_contents_observer_.reset(
      new FrontendWebContentsObserver(web_contents_));

  web_contents_->GetController().LoadURL(url, content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL, std::string());

  frontend_host_.reset(content::DevToolsClientHost::CreateDevToolsFrontendHost(
      web_contents_, this));
  file_helper_.reset(new DevToolsFileHelper(web_contents_, profile));
  file_system_indexer_ = new DevToolsFileSystemIndexer();

  g_instances.Get().push_back(this);

  // Wipe out page icon so that the default application icon is used.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  entry->GetFavicon().image = gfx::Image();
  entry->GetFavicon().valid = true;

  // Register on-load actions.
  content::Source<content::NavigationController> nav_controller_source(
      &web_contents_->GetController());
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP, nav_controller_source);
  registrar_.Add(this, chrome::NOTIFICATION_TAB_CLOSING, nav_controller_source);
  registrar_.Add(
      this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
      content::Source<ThemeService>(
          ThemeServiceFactory::GetForProfile(profile_)));

  // There is no inspected_rvh in case of shared workers.
  if (inspected_rvh)
    inspected_contents_observer_.reset(new InspectedWebContentsObserver(
        content::WebContents::FromRenderViewHost(inspected_rvh)));

  embedder_message_dispatcher_.reset(
      new DevToolsEmbedderMessageDispatcher(this));
}

// static
DevToolsWindow* DevToolsWindow::Create(
    Profile* profile,
    const GURL& frontend_url,
    content::RenderViewHost* inspected_rvh,
    DevToolsDockSide dock_side,
    bool shared_worker_frontend,
    bool external_frontend) {
  // Create WebContents with devtools.
  GURL url(GetDevToolsURL(profile, frontend_url, dock_side,
                          shared_worker_frontend,
                          external_frontend));
  return new DevToolsWindow(profile, url, inspected_rvh, dock_side);
}

// static
GURL DevToolsWindow::GetDevToolsURL(Profile* profile,
                                    const GURL& base_url,
                                    DevToolsDockSide dock_side,
                                    bool shared_worker_frontend,
                                    bool external_frontend) {
  if (base_url.SchemeIs("data"))
    return base_url;

  std::string frontend_url(
      base_url.is_empty() ? chrome::kChromeUIDevToolsURL : base_url.spec());
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile);
  DCHECK(tp);
  std::string url_string(
      frontend_url +
      ((frontend_url.find("?") == std::string::npos) ? "?" : "&") +
      "dockSide=" + SideToString(dock_side) +
      "&toolbarColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "&textColor=" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)));
  if (shared_worker_frontend)
    url_string += "&isSharedWorker=true";
  if (external_frontend)
    url_string += "&remoteFrontend=true";
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDevToolsExperiments))
    url_string += "&experiments=true";
  return GURL(url_string);
}

// static
DevToolsWindow* DevToolsWindow::FindDevToolsWindow(
    DevToolsAgentHost* agent_host) {
  DevToolsWindows* instances = &g_instances.Get();
  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if (manager->GetDevToolsAgentHostFor((*it)->frontend_host_.get()) ==
        agent_host)
      return *it;
  }
  return NULL;
}

// static
DevToolsWindow* DevToolsWindow::AsDevToolsWindow(
    content::RenderViewHost* window_rvh) {
  if (g_instances == NULL)
    return NULL;
  DevToolsWindows* instances = &g_instances.Get();
  for (DevToolsWindows::iterator it(instances->begin()); it != instances->end();
       ++it) {
    if ((*it)->web_contents_->GetRenderViewHost() == window_rvh)
      return *it;
  }
  return NULL;
}

// static
DevToolsDockSide DevToolsWindow::GetDockSideFromPrefs(Profile* profile) {
  std::string dock_side =
      profile->GetPrefs()->GetString(prefs::kDevToolsDockSide);

  // Migrate prefs.
  const char kOldPrefBottom[] = "bottom";
  const char kOldPrefRight[] = "right";
  if ((dock_side == kOldPrefBottom) || (dock_side == kOldPrefRight)) {
    if (!profile->GetPrefs()->GetBoolean(prefs::kDevToolsOpenDocked))
      return DEVTOOLS_DOCK_SIDE_UNDOCKED;
    return (dock_side == kOldPrefBottom) ?
        DEVTOOLS_DOCK_SIDE_BOTTOM : DEVTOOLS_DOCK_SIDE_RIGHT;
  }

  if (dock_side == kPrefUndocked)
    return DEVTOOLS_DOCK_SIDE_UNDOCKED;
  if (dock_side == kPrefRight)
    return DEVTOOLS_DOCK_SIDE_RIGHT;
  // Default to docked to bottom.
  return DEVTOOLS_DOCK_SIDE_BOTTOM;
}

// static
std::string DevToolsWindow::SideToString(DevToolsDockSide dock_side) {
  switch (dock_side) {
    case DEVTOOLS_DOCK_SIDE_UNDOCKED:  return kDockSideUndocked;
    case DEVTOOLS_DOCK_SIDE_RIGHT:     return kDockSideRight;
    case DEVTOOLS_DOCK_SIDE_BOTTOM:    return kDockSideBottom;
    case DEVTOOLS_DOCK_SIDE_MINIMIZED: return kDockSideMinimized;
    default:                           return kDockSideUndocked;
  }
}

// static
DevToolsDockSide DevToolsWindow::SideFromString(
    const std::string& dock_side) {
  if (dock_side == kDockSideRight)
    return DEVTOOLS_DOCK_SIDE_RIGHT;
  if (dock_side == kDockSideBottom)
    return DEVTOOLS_DOCK_SIDE_BOTTOM;
  return (dock_side == kDockSideMinimized) ?
      DEVTOOLS_DOCK_SIDE_MINIMIZED : DEVTOOLS_DOCK_SIDE_UNDOCKED;
}

void DevToolsWindow::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    if (!is_loaded_) {
      is_loaded_ = true;
      UpdateTheme();
      DoAction();
      AddDevToolsExtensionsToClient();
    }
  } else if (type == chrome::NOTIFICATION_TAB_CLOSING) {
    if (content::Source<content::NavigationController>(source).ptr() ==
        &web_contents_->GetController()) {
      // This happens when browser closes all of its tabs as a result
      // of window.Close event.
      // Notify manager that this DevToolsClientHost no longer exists and
      // initiate self-destuct here.
      content::DevToolsManager::GetInstance()->ClientHostClosing(
          frontend_host_.get());
      UpdateBrowserToolbar();
      delete this;
    }
  } else {
    DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
    UpdateTheme();
  }
}

content::WebContents* DevToolsWindow::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  if (!params.url.SchemeIs(chrome::kChromeDevToolsScheme)) {
    content::WebContents* inspected_web_contents = GetInspectedWebContents();
    return inspected_web_contents ?
        inspected_web_contents->OpenURL(params) : NULL;
  }

  content::DevToolsManager* manager = content::DevToolsManager::GetInstance();
  scoped_refptr<DevToolsAgentHost> agent_host(
      manager->GetDevToolsAgentHostFor(frontend_host_.get()));
  if (!agent_host.get())
    return NULL;
  manager->ClientHostClosing(frontend_host_.get());
  manager->RegisterDevToolsClientHostFor(agent_host.get(),
                                         frontend_host_.get());

  chrome::NavigateParams nav_params(profile_, params.url, params.transition);
  FillNavigateParamsFromOpenURLParams(&nav_params, params);
  nav_params.source_contents = source;
  nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
  nav_params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  nav_params.user_gesture = params.user_gesture;
  chrome::Navigate(&nav_params);
  return nav_params.target_contents;
}

void DevToolsWindow::AddNewContents(content::WebContents* source,
                                    content::WebContents* new_contents,
                                    WindowOpenDisposition disposition,
                                    const gfx::Rect& initial_pos,
                                    bool user_gesture,
                                    bool* was_blocked) {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->GetDelegate()->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture,
        was_blocked);
  }
}

void DevToolsWindow::CloseContents(content::WebContents* source) {
  CHECK(IsDocked());
  // Update dev tools to reflect removed dev tools window.
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateDevTools();
  // In case of docked web_contents_, we own it so delete here.
  delete web_contents_;

  delete this;
}

void DevToolsWindow::BeforeUnloadFired(content::WebContents* tab,
                                       bool proceed,
                                       bool* proceed_to_fire_unload) {
  if (proceed) {
    content::DevToolsManager::GetInstance()->ClientHostClosing(
        frontend_host_.get());
  }
  *proceed_to_fire_unload = proceed;
}

bool DevToolsWindow::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event,
    bool* is_keyboard_shortcut) {
  if (IsDocked()) {
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window) {
      return inspected_window->PreHandleKeyboardEvent(event,
                                                      is_keyboard_shortcut);
    }
  }
  return false;
}

void DevToolsWindow::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (IsDocked()) {
    if (event.windowsKeyCode == 0x08) {
      // Do not navigate back in history on Windows (http://crbug.com/74156).
      return;
    }
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->HandleKeyboardEvent(event);
  }
}

content::JavaScriptDialogManager* DevToolsWindow::GetJavaScriptDialogManager() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  return (inspected_web_contents && inspected_web_contents->GetDelegate()) ?
      inspected_web_contents->GetDelegate()->GetJavaScriptDialogManager() :
      content::WebContentsDelegate::GetJavaScriptDialogManager();
}

content::ColorChooser* DevToolsWindow::OpenColorChooser(
    content::WebContents* web_contents,
    SkColor initial_color) {
  return chrome::ShowColorChooser(web_contents, initial_color);
}

void DevToolsWindow::RunFileChooser(content::WebContents* web_contents,
                                    const content::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(web_contents, params);
}

void DevToolsWindow::WebContentsFocused(content::WebContents* contents) {
  Browser* inspected_browser = NULL;
  int inspected_tab_index = -1;
  if (IsDocked() && FindInspectedBrowserAndTabIndex(&inspected_browser,
                                                    &inspected_tab_index))
    inspected_browser->window()->WebContentsFocused(contents);
}

void DevToolsWindow::DispatchOnEmbedder(const std::string& message) {
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

  std::string error = embedder_message_dispatcher_->Dispatch(method, params);
  if (id) {
    scoped_ptr<base::Value> id_value(base::Value::CreateIntegerValue(id));
    scoped_ptr<base::Value> error_value(base::Value::CreateStringValue(error));
    CallClientFunction("InspectorFrontendAPI.embedderMessageAck",
                       id_value.get(), error_value.get(), NULL);
  }
}

void DevToolsWindow::ActivateWindow() {
  if (IsDocked() && GetInspectedBrowserWindow())
    web_contents_->GetView()->Focus();
  else if (!IsDocked() && !browser_->window()->IsActive())
    browser_->window()->Activate();
}

void DevToolsWindow::ActivateContents(content::WebContents* contents) {
  if (IsDocked()) {
    content::WebContents* inspected_tab = this->GetInspectedWebContents();
    inspected_tab->GetDelegate()->ActivateContents(inspected_tab);
  } else {
    browser_->window()->Activate();
  }
}

void DevToolsWindow::CloseWindow() {
  DCHECK(IsDocked());
  web_contents_->GetRenderViewHost()->FirePageBeforeUnload(false);
}

void DevToolsWindow::SetWindowBounds(int x, int y, int width, int height) {
  if (!IsDocked())
    browser_->window()->SetBounds(gfx::Rect(x, y, width, height));
}

void DevToolsWindow::MoveWindow(int x, int y) {
  if (!IsDocked()) {
    gfx::Rect bounds = browser_->window()->GetBounds();
    bounds.Offset(x, y);
    browser_->window()->SetBounds(bounds);
  }
}

void DevToolsWindow::SetDockSide(const std::string& side) {
  DevToolsDockSide requested_side = SideFromString(side);
  bool dock_requested = requested_side != DEVTOOLS_DOCK_SIDE_UNDOCKED;
  bool is_docked = IsDocked();

  if (dock_requested &&
      (!GetInspectedWebContents() || !GetInspectedBrowserWindow() ||
       IsInspectedBrowserPopup())) {
      // Cannot dock, avoid window flashing due to close-reopen cycle.
    return;
  }

  if ((dock_side_ != DEVTOOLS_DOCK_SIDE_MINIMIZED) &&
      (requested_side == DEVTOOLS_DOCK_SIDE_MINIMIZED))
    dock_side_before_minimized_ = dock_side_;

  dock_side_ = requested_side;
  if (dock_requested && !is_docked) {
    // Detach window from the external devtools browser. It will lead to
    // the browser object's close and delete. Remove observer first.
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    tab_strip_model->DetachWebContentsAt(
        tab_strip_model->GetIndexOfWebContents(web_contents_));
    browser_ = NULL;
  } else if (!dock_requested && is_docked) {
    // Update inspected window to hide split and reset it.
    BrowserWindow* inspected_window = GetInspectedBrowserWindow();
    if (inspected_window)
      inspected_window->UpdateDevTools();
  }

  if (dock_side_ != DEVTOOLS_DOCK_SIDE_MINIMIZED) {
    std::string pref_value = kPrefBottom;
    switch (dock_side_) {
      case DEVTOOLS_DOCK_SIDE_UNDOCKED:
          pref_value = kPrefUndocked;
          break;
      case DEVTOOLS_DOCK_SIDE_RIGHT:
          pref_value = kPrefRight;
          break;
      case DEVTOOLS_DOCK_SIDE_BOTTOM:
          pref_value = kPrefBottom;
          break;
      case DEVTOOLS_DOCK_SIDE_MINIMIZED:
          // We don't persist minimized state.
          break;
    }
    profile_->GetPrefs()->SetString(prefs::kDevToolsDockSide, pref_value);
  }

  Show(DEVTOOLS_TOGGLE_ACTION_SHOW);
}

void DevToolsWindow::OpenInNewTab(const std::string& url) {
  content::OpenURLParams params(
      GURL(url), content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false);
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    inspected_web_contents->OpenURL(params);
  } else {
    chrome::HostDesktopType host_desktop_type;
    if (browser_) {
      host_desktop_type = browser_->host_desktop_type();
    } else {
      // There should always be a browser when there are no inspected web
      // contents.
      NOTREACHED();
      host_desktop_type = chrome::GetActiveDesktop();
    }

    const BrowserList* browser_list =
        BrowserList::GetInstance(host_desktop_type);
    for (BrowserList::const_iterator it = browser_list->begin();
         it != browser_list->end(); ++it) {
      if ((*it)->type() == Browser::TYPE_TABBED) {
        (*it)->OpenURL(params);
        break;
      }
    }
  }
}

void DevToolsWindow::SaveToFile(const std::string& url,
                                const std::string& content,
                                bool save_as) {
  file_helper_->Save(url, content, save_as,
                     base::Bind(&DevToolsWindow::FileSavedAs,
                                weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindow::AppendToFile(const std::string& url,
                                  const std::string& content) {
  file_helper_->Append(url, content,
                       base::Bind(&DevToolsWindow::AppendedTo,
                                  weak_factory_.GetWeakPtr(), url));
}

void DevToolsWindow::RequestFileSystems() {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->RequestFileSystems(base::Bind(
      &DevToolsWindow::FileSystemsLoaded, weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::AddFileSystem() {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->AddFileSystem(
      base::Bind(&DevToolsWindow::FileSystemAdded, weak_factory_.GetWeakPtr()),
      base::Bind(&DevToolsWindow::ShowDevToolsConfirmInfoBar,
                 weak_factory_.GetWeakPtr()));
}

void DevToolsWindow::RemoveFileSystem(const std::string& file_system_path) {
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  file_helper_->RemoveFileSystem(file_system_path);
  StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.fileSystemRemoved",
                     &file_system_path_value, NULL, NULL);
}

void DevToolsWindow::IndexPath(int request_id,
                               const std::string& file_system_path) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    IndexingDone(request_id, file_system_path);
    return;
  }
  indexing_jobs_[request_id] =
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob>(
          file_system_indexer_->IndexPath(
              file_system_path,
              Bind(&DevToolsWindow::IndexingTotalWorkCalculated,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindow::IndexingWorked,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path),
              Bind(&DevToolsWindow::IndexingDone,
                   weak_factory_.GetWeakPtr(),
                   request_id,
                   file_system_path)));
}

void DevToolsWindow::StopIndexing(int request_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  IndexingJobsMap::iterator it = indexing_jobs_.find(request_id);
  if (it == indexing_jobs_.end())
    return;
  it->second->Stop();
  indexing_jobs_.erase(it);
}

void DevToolsWindow::SearchInPath(int request_id,
                                  const std::string& file_system_path,
                                  const std::string& query) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(web_contents_->GetURL().SchemeIs(chrome::kChromeDevToolsScheme));
  if (!file_helper_->IsFileSystemAdded(file_system_path)) {
    SearchCompleted(request_id, file_system_path, std::vector<std::string>());
    return;
  }
  file_system_indexer_->SearchInPath(file_system_path,
                                     query,
                                     Bind(&DevToolsWindow::SearchCompleted,
                                          weak_factory_.GetWeakPtr(),
                                          request_id,
                                          file_system_path));
}

void DevToolsWindow::FileSavedAs(const std::string& url) {
  StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.savedURL", &url_value, NULL, NULL);
}

void DevToolsWindow::AppendedTo(const std::string& url) {
  StringValue url_value(url);
  CallClientFunction("InspectorFrontendAPI.appendedToURL", &url_value, NULL,
                     NULL);
}

void DevToolsWindow::FileSystemsLoaded(
    const std::vector<DevToolsFileHelper::FileSystem>& file_systems) {
  ListValue file_systems_value;
  for (size_t i = 0; i < file_systems.size(); ++i)
    file_systems_value.Append(CreateFileSystemValue(file_systems[i]));
  CallClientFunction("InspectorFrontendAPI.fileSystemsLoaded",
                     &file_systems_value, NULL, NULL);
}

void DevToolsWindow::FileSystemAdded(
    const DevToolsFileHelper::FileSystem& file_system) {
  StringValue error_string_value((std::string()));
  DictionaryValue* file_system_value = NULL;
  if (!file_system.file_system_path.empty())
    file_system_value = CreateFileSystemValue(file_system);
  CallClientFunction("InspectorFrontendAPI.fileSystemAdded",
                     &error_string_value, file_system_value, NULL);
  if (file_system_value)
    delete file_system_value;
}

void DevToolsWindow::IndexingTotalWorkCalculated(
    int request_id,
    const std::string& file_system_path,
    int total_work) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  StringValue file_system_path_value(file_system_path);
  base::FundamentalValue total_work_value(total_work);
  CallClientFunction("InspectorFrontendAPI.indexingTotalWorkCalculated",
                     &request_id_value, &file_system_path_value,
                     &total_work_value);
}

void DevToolsWindow::IndexingWorked(int request_id,
                                    const std::string& file_system_path,
                                    int worked) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  StringValue file_system_path_value(file_system_path);
  base::FundamentalValue worked_value(worked);
  CallClientFunction("InspectorFrontendAPI.indexingWorked", &request_id_value,
                     &file_system_path_value, &worked_value);
}

void DevToolsWindow::IndexingDone(int request_id,
                                  const std::string& file_system_path) {
  indexing_jobs_.erase(request_id);
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  base::FundamentalValue request_id_value(request_id);
  StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.indexingDone", &request_id_value,
                     &file_system_path_value, NULL);
}

void DevToolsWindow::SearchCompleted(
    int request_id,
    const std::string& file_system_path,
    const std::vector<std::string>& file_paths) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  ListValue file_paths_value;
  for (std::vector<std::string>::const_iterator it(file_paths.begin());
       it != file_paths.end(); ++it) {
    file_paths_value.AppendString(*it);
  }
  base::FundamentalValue request_id_value(request_id);
  StringValue file_system_path_value(file_system_path);
  CallClientFunction("InspectorFrontendAPI.searchCompleted", &request_id_value,
                     &file_system_path_value, &file_paths_value);
}

void DevToolsWindow::ShowDevToolsConfirmInfoBar(
    const string16& message,
    const InfoBarCallback& callback) {
  DevToolsConfirmInfoBarDelegate::Create(
      IsDocked() ?
          InfoBarService::FromWebContents(GetInspectedWebContents()) :
          InfoBarService::FromWebContents(web_contents_),
      callback, message);
}

void DevToolsWindow::CreateDevToolsBrowser() {
  std::string wp_key = GetDevToolsWindowPlacementPrefKey();
  PrefService* prefs = profile_->GetPrefs();
  const DictionaryValue* wp_pref = prefs->GetDictionary(wp_key.c_str());
  if (!wp_pref || wp_pref->empty()) {
    DictionaryPrefUpdate update(prefs, wp_key.c_str());
    DictionaryValue* defaults = update.Get();
    defaults->SetInteger("left", 100);
    defaults->SetInteger("top", 100);
    defaults->SetInteger("right", 740);
    defaults->SetInteger("bottom", 740);
    defaults->SetBoolean("maximized", false);
    defaults->SetBoolean("always_on_top", false);
  }

  browser_ = new Browser(Browser::CreateParams::CreateForDevTools(
      profile_,
      chrome::GetHostDesktopTypeForNativeView(
          web_contents_->GetView()->GetNativeView())));
  browser_->tab_strip_model()->AddWebContents(
      web_contents_, -1, content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);
  GetRenderViewHost()->SyncRendererPrefs();
}

bool DevToolsWindow::FindInspectedBrowserAndTabIndex(Browser** browser,
                                                     int* tab) {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (!inspected_web_contents)
    return false;

  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    int tab_index = it->tab_strip_model()->GetIndexOfWebContents(
        inspected_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      *browser = *it;
      *tab = tab_index;
      return true;
    }
  }
  return false;
}

BrowserWindow* DevToolsWindow::GetInspectedBrowserWindow() {
  Browser* browser = NULL;
  int tab;
  return FindInspectedBrowserAndTabIndex(&browser, &tab) ?
      browser->window() : NULL;
}

bool DevToolsWindow::IsInspectedBrowserPopup() {
  Browser* browser = NULL;
  int tab;
  return FindInspectedBrowserAndTabIndex(&browser, &tab) &&
      browser->is_type_popup();
}

void DevToolsWindow::UpdateFrontendDockSide() {
  base::StringValue dock_side(SideToString(dock_side_));
  CallClientFunction("InspectorFrontendAPI.setDockSide", &dock_side, NULL,
                     NULL);
  base::FundamentalValue docked(IsDocked());
  CallClientFunction("InspectorFrontendAPI.setAttachedWindow", &docked, NULL,
                     NULL);
}

void DevToolsWindow::ScheduleAction(DevToolsToggleAction action) {
  action_on_load_ = action;
  if (is_loaded_)
    DoAction();
}

void DevToolsWindow::DoAction() {
  UpdateFrontendDockSide();
  switch (action_on_load_) {
    case DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE:
      CallClientFunction("InspectorFrontendAPI.showConsole", NULL, NULL, NULL);
      break;

    case DEVTOOLS_TOGGLE_ACTION_INSPECT:
      CallClientFunction("InspectorFrontendAPI.enterInspectElementMode", NULL,
                         NULL, NULL);
      break;

    case DEVTOOLS_TOGGLE_ACTION_SHOW:
    case DEVTOOLS_TOGGLE_ACTION_TOGGLE:
      // Do nothing.
      break;

    default:
      NOTREACHED();
      break;
  }
  action_on_load_ = DEVTOOLS_TOGGLE_ACTION_SHOW;
}

void DevToolsWindow::UpdateTheme() {
  ThemeService* tp = ThemeServiceFactory::GetForProfile(profile_);
  DCHECK(tp);

  std::string command("InspectorFrontendAPI.setToolbarColors(\"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_TOOLBAR)) +
      "\", \"" +
      SkColorToRGBAString(tp->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT)) +
      "\")");
  web_contents_->GetRenderViewHost()->ExecuteJavascriptInWebFrame(
      string16(), ASCIIToUTF16(command));
}

void DevToolsWindow::AddDevToolsExtensionsToClient() {
  content::WebContents* inspected_web_contents = GetInspectedWebContents();
  if (inspected_web_contents) {
    SessionTabHelper* session_tab_helper =
        SessionTabHelper::FromWebContents(inspected_web_contents);
    if (session_tab_helper) {
      base::FundamentalValue tabId(session_tab_helper->session_id().id());
      CallClientFunction("WebInspector.setInspectedTabId", &tabId, NULL, NULL);
    }
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  const ExtensionService* extension_service = extensions::ExtensionSystem::Get(
      profile->GetOriginalProfile())->extension_service();
  if (!extension_service)
    return;
  const ExtensionSet* extensions = extension_service->extensions();

  ListValue results;
  for (ExtensionSet::const_iterator extension(extensions->begin());
       extension != extensions->end(); ++extension) {
    if (extensions::ManifestURL::GetDevToolsPage(extension->get()).is_empty())
      continue;
    DictionaryValue* extension_info = new DictionaryValue();
    extension_info->Set(
        "startPage",
        new StringValue(
            extensions::ManifestURL::GetDevToolsPage(extension->get()).spec()));
    extension_info->Set("name", new StringValue((*extension)->name()));
    extension_info->Set(
        "exposeExperimentalAPIs",
        new base::FundamentalValue((*extension)->HasAPIPermission(
            extensions::APIPermission::kExperimental)));
    results.Append(extension_info);
  }
  CallClientFunction("WebInspector.addExtensions", &results, NULL, NULL);
}

void DevToolsWindow::CallClientFunction(const std::string& function_name,
                                        const Value* arg1,
                                        const Value* arg2,
                                        const Value* arg3) {
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
  string16 javascript = ASCIIToUTF16(function_name + "(" + params + ");");
  web_contents_->GetRenderViewHost()->
      ExecuteJavascriptInWebFrame(string16(), javascript);
}

void DevToolsWindow::UpdateBrowserToolbar() {
  BrowserWindow* inspected_window = GetInspectedBrowserWindow();
  if (inspected_window)
    inspected_window->UpdateToolbar(NULL);
}

bool DevToolsWindow::IsDocked() {
  return dock_side_ != DEVTOOLS_DOCK_SIDE_UNDOCKED;
}

void DevToolsWindow::Restore() {
  if (dock_side_ == DEVTOOLS_DOCK_SIDE_MINIMIZED)
    SetDockSide(SideToString(dock_side_before_minimized_));
}

content::WebContents* DevToolsWindow::GetInspectedWebContents() {
  return inspected_contents_observer_ ?
      inspected_contents_observer_->web_contents() : NULL;
}
