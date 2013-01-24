// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/panels/native_panel.h"
#include "chrome/browser/ui/panels/panel_collection.h"
#include "chrome/browser/ui/panels/panel_host.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/stacked_panel_collection.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"

using content::RenderViewHost;
using content::UserMetricsAction;

namespace panel_internal {

class PanelExtensionWindowController : public extensions::WindowController {
 public:
  PanelExtensionWindowController(Panel* panel, Profile* profile);
  virtual ~PanelExtensionWindowController();

  // Overridden from extensions::WindowController.
  virtual int GetWindowId() const OVERRIDE;
  virtual std::string GetWindowTypeText() const OVERRIDE;
  virtual base::DictionaryValue* CreateWindowValueWithTabs(
      const extensions::Extension* extension) const OVERRIDE;
  virtual bool CanClose(Reason* reason) const OVERRIDE;
  virtual void SetFullscreenMode(bool is_fullscreen,
                                 const GURL& extension_url) const OVERRIDE;
  virtual bool IsVisibleToExtension(
      const extensions::Extension* extension) const OVERRIDE;

 private:
  Panel* panel_;  // Weak pointer. Owns us.
  DISALLOW_COPY_AND_ASSIGN(PanelExtensionWindowController);
};

PanelExtensionWindowController::PanelExtensionWindowController(
    Panel* panel, Profile* profile)
    : extensions::WindowController(panel, profile),
      panel_(panel) {
  extensions::WindowControllerList::GetInstance()->AddExtensionWindow(this);
}

PanelExtensionWindowController::~PanelExtensionWindowController() {
  extensions::WindowControllerList::GetInstance()->RemoveExtensionWindow(this);
}

int PanelExtensionWindowController::GetWindowId() const {
  return static_cast<int>(panel_->session_id().id());
}

std::string PanelExtensionWindowController::GetWindowTypeText() const {
  return extensions::tabs_constants::kWindowTypeValuePanel;
}

base::DictionaryValue*
PanelExtensionWindowController::CreateWindowValueWithTabs(
    const extensions::Extension* extension) const {
  base::DictionaryValue* result = CreateWindowValue();

  DCHECK(IsVisibleToExtension(extension));
  content::WebContents* web_contents = panel_->GetWebContents();
  if (web_contents) {
    DictionaryValue* tab_value = new DictionaryValue();
    tab_value->SetInteger(extensions::tabs_constants::kIdKey,
                          SessionID::IdForTab(web_contents));
    tab_value->SetInteger(extensions::tabs_constants::kIndexKey, 0);
    tab_value->SetInteger(extensions::tabs_constants::kWindowIdKey,
                          SessionID::IdForWindowContainingTab(web_contents));
    tab_value->SetString(
        extensions::tabs_constants::kUrlKey, web_contents->GetURL().spec());
    tab_value->SetString(extensions::tabs_constants::kStatusKey,
         ExtensionTabUtil::GetTabStatusText(web_contents->IsLoading()));
    tab_value->SetBoolean(
        extensions::tabs_constants::kActiveKey, panel_->IsActive());
    tab_value->SetBoolean(extensions::tabs_constants::kSelectedKey, true);
    tab_value->SetBoolean(extensions::tabs_constants::kHighlightedKey, true);
    tab_value->SetBoolean(extensions::tabs_constants::kPinnedKey, false);
    tab_value->SetString(
        extensions::tabs_constants::kTitleKey, web_contents->GetTitle());
    tab_value->SetBoolean(
        extensions::tabs_constants::kIncognitoKey,
        web_contents->GetBrowserContext()->IsOffTheRecord());

    base::ListValue* tab_list = new ListValue();
    tab_list->Append(tab_value);
    result->Set(extensions::tabs_constants::kTabsKey, tab_list);
  }
  return result;
}

bool PanelExtensionWindowController::CanClose(Reason* reason) const {
  return true;
}

void PanelExtensionWindowController::SetFullscreenMode(
    bool is_fullscreen, const GURL& extension_url) const {
  // Do nothing. Panels cannot be fullscreen.
}

bool PanelExtensionWindowController::IsVisibleToExtension(
    const extensions::Extension* extension) const {
  return extension->id() == panel_->extension_id();
}

}  // namespace internal

Panel::Panel(const std::string& app_name,
             const gfx::Size& min_size, const gfx::Size& max_size)
    : app_name_(app_name),
      profile_(NULL),
      collection_(NULL),
      initialized_(false),
      min_size_(min_size),
      max_size_(max_size),
      max_size_policy_(DEFAULT_MAX_SIZE),
      auto_resizable_(false),
      in_preview_mode_(false),
      native_panel_(NULL),
      attention_mode_(USE_PANEL_ATTENTION),
      expansion_state_(EXPANDED),
      command_updater_(this),
      ALLOW_THIS_IN_INITIALIZER_LIST(image_loader_ptr_factory_(this)) {
}

Panel::~Panel() {
  DCHECK(!collection_);
  // Invoked by native panel destructor. Do not access native_panel_ here.
  browser::EndKeepAlive();  // Remove shutdown prevention.
}

void Panel::Initialize(Profile* profile, const GURL& url,
                       const gfx::Rect& bounds) {
  DCHECK(!initialized_);
  DCHECK(!collection_);  // Cannot be added to a collection until fully created.
  DCHECK_EQ(EXPANDED, expansion_state_);
  DCHECK(!bounds.IsEmpty());
  initialized_ = true;
  profile_ = profile;
  full_size_ = bounds.size();
  native_panel_ = CreateNativePanel(this, bounds);

  extension_window_controller_.reset(
      new panel_internal::PanelExtensionWindowController(this, profile));

  InitCommandState();

  // Set up hosting for web contents.
  panel_host_.reset(new PanelHost(this, profile));
  panel_host_->Init(url);
  native_panel_->AttachWebContents(GetWebContents());

  // Close when the extension is unloaded or the browser is exiting.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                    ThemeServiceFactory::GetForProfile(profile)));

  // Prevent the browser process from shutting down while this window is open.
  browser::StartKeepAlive();

  UpdateAppIcon();
}

void Panel::InitCommandState() {
  // All supported commands whose state isn't set automagically some other way
  // (like Stop during a page load) must have their state initialized here,
  // otherwise they will be forever disabled.

  // Navigation commands
  command_updater_.UpdateCommandEnabled(IDC_RELOAD, true);
  command_updater_.UpdateCommandEnabled(IDC_RELOAD_IGNORING_CACHE, true);

  // Window management commands
  command_updater_.UpdateCommandEnabled(IDC_CLOSE_WINDOW, true);
  command_updater_.UpdateCommandEnabled(IDC_EXIT, true);

  // Zoom
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MENU, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_PLUS, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_NORMAL, true);
  command_updater_.UpdateCommandEnabled(IDC_ZOOM_MINUS, true);

  // Clipboard
  command_updater_.UpdateCommandEnabled(IDC_COPY, true);
  command_updater_.UpdateCommandEnabled(IDC_CUT, true);
  command_updater_.UpdateCommandEnabled(IDC_PASTE, true);

  // DevTools
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS, true);
  command_updater_.UpdateCommandEnabled(IDC_DEV_TOOLS_CONSOLE, true);
}

void Panel::OnNativePanelClosed() {
  // Ensure previously enqueued OnImageLoaded callbacks are ignored.
  image_loader_ptr_factory_.InvalidateWeakPtrs();
  registrar_.RemoveAll();
  manager()->OnPanelClosed(this);
  DCHECK(!collection_);
}

PanelManager* Panel::manager() const {
  return PanelManager::GetInstance();
}

CommandUpdater* Panel::command_updater() {
  return &command_updater_;
}

Profile* Panel::profile() const {
  return profile_;
}

const std::string Panel::extension_id() const {
  return web_app::GetExtensionIdFromApplicationName(app_name_);
}

StackedPanelCollection* Panel::stack() const {
  return collection_ && collection_->type() == PanelCollection::STACKED ?
      static_cast<StackedPanelCollection*>(collection_) : NULL;
}

content::WebContents* Panel::GetWebContents() const {
  return panel_host_.get() ? panel_host_->web_contents() : NULL;
}

bool Panel::CanShowMinimizeButton() const {
  return collection_ && collection_->CanShowMinimizeButton(this);
}

bool Panel::CanShowRestoreButton() const {
  return collection_ && collection_->CanShowRestoreButton(this);
}

panel::Resizability Panel::CanResizeByMouse() const {
  if (!collection_)
    return panel::NOT_RESIZABLE;

  return collection_->GetPanelResizability(this);
}

void Panel::SetPanelBounds(const gfx::Rect& bounds) {
  if (bounds != native_panel_->GetPanelBounds())
    native_panel_->SetPanelBounds(bounds);
}

void Panel::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  native_panel_->SetPanelBoundsInstantly(bounds);
}

void Panel::LimitSizeToDisplayArea(const gfx::Rect& display_area) {
  int max_width = manager()->GetMaxPanelWidth();
  int max_height = manager()->GetMaxPanelHeight();

  // If the custom max size is used, ensure that it does not exceed the display
  // area.
  if (max_size_policy_ == CUSTOM_MAX_SIZE) {
    int current_max_width = max_size_.width();
    if (current_max_width > max_width)
      max_width = std::min(current_max_width, display_area.width());
    int current_max_height = max_size_.height();
    if (current_max_height > max_height)
      max_height = std::min(current_max_height, display_area.height());
  }

  SetSizeRange(min_size_, gfx::Size(max_width, max_height));

  // Ensure that full size does not exceed max size.
  full_size_ = ClampSize(full_size_);
}

void Panel::SetAutoResizable(bool resizable) {
  if (auto_resizable_ == resizable)
    return;

  auto_resizable_ = resizable;
  content::WebContents* web_contents = GetWebContents();
  if (auto_resizable_) {
    if (web_contents)
      EnableWebContentsAutoResize(web_contents);
  } else {
    if (web_contents) {
      registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
                        content::Source<content::WebContents>(web_contents));

      // NULL might be returned if the tab has not been added.
      RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
      if (render_view_host)
        render_view_host->DisableAutoResize(full_size_);
    }
  }
}

void Panel::SetSizeRange(const gfx::Size& min_size, const gfx::Size& max_size) {
  if (min_size == min_size_ && max_size == max_size_)
    return;

  DCHECK(min_size.width() <= max_size.width());
  DCHECK(min_size.height() <= max_size.height());
  min_size_ = min_size;
  max_size_ = max_size;

  ConfigureAutoResize(GetWebContents());
}

void Panel::IncreaseMaxSize(const gfx::Size& desired_panel_size) {
  gfx::Size new_max_size = max_size_;
  if (new_max_size.width() < desired_panel_size.width())
    new_max_size.set_width(desired_panel_size.width());
  if (new_max_size.height() < desired_panel_size.height())
    new_max_size.set_height(desired_panel_size.height());

  SetSizeRange(min_size_, new_max_size);
}

gfx::Size Panel::ClampSize(const gfx::Size& size) const {
  // The panel width:
  // * cannot grow or shrink to go beyond [min_width, max_width]
  int new_width = size.width();
  if (new_width > max_size_.width())
    new_width = max_size_.width();
  if (new_width < min_size_.width())
    new_width = min_size_.width();

  // The panel height:
  // * cannot grow or shrink to go beyond [min_height, max_height]
  int new_height = size.height();
  if (new_height > max_size_.height())
    new_height = max_size_.height();
  if (new_height < min_size_.height())
    new_height = min_size_.height();

  return gfx::Size(new_width, new_height);
}

void Panel::HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event) {
  native_panel_->HandlePanelKeyboardEvent(event);
}

void Panel::SetAlwaysOnTop(bool on_top) {
  native_panel_->SetPanelAlwaysOnTop(on_top);
}

void Panel::EnableResizeByMouse(bool enable) {
  DCHECK(native_panel_);
  native_panel_->EnableResizeByMouse(enable);
}

void Panel::UpdateMinimizeRestoreButtonVisibility() {
  native_panel_->UpdatePanelMinimizeRestoreButtonVisibility();
}

void Panel::SetPreviewMode(bool in_preview) {
  DCHECK_NE(in_preview_mode_, in_preview);
  in_preview_mode_ = in_preview;
}

void Panel::SetExpansionState(ExpansionState new_state) {
  if (expansion_state_ == new_state)
    return;
  native_panel_->PanelExpansionStateChanging(expansion_state_, new_state);
  expansion_state_ = new_state;

  manager()->OnPanelExpansionStateChanged(this);

  DCHECK(initialized_ && collection_ != NULL);
  native_panel_->PreventActivationByOS(collection_->IsPanelMinimized(this));
  UpdateMinimizeRestoreButtonVisibility();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

bool Panel::IsDrawingAttention() const {
  return native_panel_->IsDrawingAttention();
}

void Panel::FullScreenModeChanged(bool is_full_screen) {
  native_panel_->FullScreenModeChanged(is_full_screen);
}

void Panel::Show() {
  if (manager()->display_settings_provider()->is_full_screen() || !collection_)
    return;

  native_panel_->ShowPanel();
}

void Panel::ShowInactive() {
  if (manager()->display_settings_provider()->is_full_screen() || !collection_)
    return;

  native_panel_->ShowPanelInactive();
}

void Panel::Hide() {
  // Not implemented.
}

void Panel::SetBounds(const gfx::Rect& bounds) {
  // Ignore bounds position as the panel manager controls all positioning.
  if (!collection_)
    return;
  collection_->ResizePanelWindow(this, bounds.size());
  SetAutoResizable(false);
}

// Close() may be called multiple times if the panel window is not ready to
// close on the first attempt.
void Panel::Close() {
  native_panel_->ClosePanel();
}

void Panel::Activate() {
  if (!collection_)
    return;

  collection_->ActivatePanel(this);
  native_panel_->ActivatePanel();
}

void Panel::Deactivate() {
  native_panel_->DeactivatePanel();
}

bool Panel::IsActive() const {
  return native_panel_->IsPanelActive();
}

void Panel::FlashFrame(bool draw_attention) {
  if (IsDrawingAttention() == draw_attention || !collection_)
    return;

  // Don't draw attention for an active panel.
  if (draw_attention && IsActive())
    return;

  // Invoking native panel to draw attention must be done before informing the
  // panel collection because it needs to check internal state of the panel to
  // determine if the panel has been drawing attention.
  native_panel_->DrawAttention(draw_attention);
  collection_->OnPanelAttentionStateChanged(this);
}

bool Panel::IsAlwaysOnTop() const {
  return native_panel_->IsPanelAlwaysOnTop();
}

gfx::NativeWindow Panel::GetNativeWindow() {
  return native_panel_->GetNativePanelWindow();
}

gfx::Rect Panel::GetRestoredBounds() const {
  gfx::Rect bounds = native_panel_->GetPanelBounds();
  bounds.set_y(bounds.bottom() - full_size_.height());
  bounds.set_x(bounds.right() - full_size_.width());
  bounds.set_size(full_size_);
  return bounds;
}

gfx::Rect Panel::GetBounds() const {
  return native_panel_->GetPanelBounds();
}

int Panel::TitleOnlyHeight() const {
  return native_panel_->TitleOnlyHeight();
}

bool Panel::IsMaximized() const {
  // Size of panels is managed by PanelManager, they are never 'zoomed'.
  return false;
}

bool Panel::IsMinimized() const {
  return !collection_ || collection_->IsPanelMinimized(this);
}

void Panel::Maximize() {
  Restore();
}

void Panel::Minimize() {
  if (collection_)
    collection_->MinimizePanel(this);
}

void Panel::Restore() {
  if (collection_)
    collection_->RestorePanel(this);
}

bool Panel::IsFullscreen() const {
  return false;
}

void Panel::OnContentsAutoResized(const gfx::Size& new_content_size) {
  DCHECK(auto_resizable_);
  if (!collection_)
    return;

  gfx::Size new_window_size =
      native_panel_->WindowSizeFromContentSize(new_content_size);

  // Ignore content auto resizes until window frame size is known.
  // This reduces extra resizes when panel is first shown.
  // After window frame size is known, it will trigger another content
  // auto resize.
  if (new_content_size == new_window_size)
    return;

  collection_->ResizePanelWindow(this, new_window_size);
}

void Panel::OnWindowResizedByMouse(const gfx::Rect& new_bounds) {
  if (collection_)
    collection_->OnPanelResizedByMouse(this, new_bounds);
}

void Panel::EnableWebContentsAutoResize(content::WebContents* web_contents) {
  DCHECK(web_contents);
  ConfigureAutoResize(web_contents);

  // We also need to know when the render view host changes in order
  // to turn on auto-resize notifications in the new render view host.
  if (!registrar_.IsRegistered(
          this, content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
          content::Source<content::WebContents>(web_contents))) {
    registrar_.Add(
        this,
        content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
        content::Source<content::WebContents>(web_contents));
  }
}

void Panel::ExecuteCommandWithDisposition(int id,
                                          WindowOpenDisposition disposition) {
  DCHECK(command_updater_.IsCommandEnabled(id)) << "Invalid/disabled command "
                                                << id;

  if (!GetWebContents())
    return;

  switch (id) {
    // Navigation
    case IDC_RELOAD:
      panel_host_->Reload();
      break;
    case IDC_RELOAD_IGNORING_CACHE:
      panel_host_->ReloadIgnoringCache();
      break;
    case IDC_STOP:
      panel_host_->StopLoading();
      break;

    // Window management
    case IDC_CLOSE_WINDOW:
      content::RecordAction(UserMetricsAction("CloseWindow"));
      Close();
      break;
    case IDC_EXIT:
      content::RecordAction(UserMetricsAction("Exit"));
      browser::AttemptUserExit();
      break;

    // Clipboard
    case IDC_COPY:
      content::RecordAction(UserMetricsAction("Copy"));
      native_panel_->PanelCopy();
      break;
    case IDC_CUT:
      content::RecordAction(UserMetricsAction("Cut"));
      native_panel_->PanelCut();
      break;
    case IDC_PASTE:
      content::RecordAction(UserMetricsAction("Paste"));
      native_panel_->PanelPaste();
      break;

    // Zoom
    case IDC_ZOOM_PLUS:
      panel_host_->Zoom(content::PAGE_ZOOM_IN);
      break;
    case IDC_ZOOM_NORMAL:
      panel_host_->Zoom(content::PAGE_ZOOM_RESET);
      break;
    case IDC_ZOOM_MINUS:
      panel_host_->Zoom(content::PAGE_ZOOM_OUT);
      break;

    // DevTools
    case IDC_DEV_TOOLS:
      content::RecordAction(UserMetricsAction("DevTools_ToggleWindow"));
      DevToolsWindow::ToggleDevToolsWindow(
          GetWebContents()->GetRenderViewHost(),
          true,
          DEVTOOLS_TOGGLE_ACTION_SHOW);
      break;
    case IDC_DEV_TOOLS_CONSOLE:
      content::RecordAction(UserMetricsAction("DevTools_ToggleConsole"));
      DevToolsWindow::ToggleDevToolsWindow(
          GetWebContents()->GetRenderViewHost(),
          true,
          DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);
      break;

    default:
      LOG(WARNING) << "Received unimplemented command: " << id;
      break;
  }
}

bool Panel::ExecuteCommandIfEnabled(int id) {
  if (command_updater()->SupportsCommand(id) &&
      command_updater()->IsCommandEnabled(id)) {
    ExecuteCommandWithDisposition(id, CURRENT_TAB);
    return true;
  }
  return false;
}

void Panel::Observe(int type,
                    const content::NotificationSource& source,
                    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_SWAPPED:
      ConfigureAutoResize(content::Source<content::WebContents>(source).ptr());
      break;
    case chrome::NOTIFICATION_EXTENSION_UNLOADED:
      if (content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension->id() == extension_id())
        Close();
      break;
    case chrome::NOTIFICATION_APP_TERMINATING:
      Close();
      break;
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      native_panel_->NotifyPanelOnUserChangedTheme();
      break;
    default:
      NOTREACHED() << "Received unexpected notification " << type;
  }
}

void Panel::OnActiveStateChanged(bool active) {
  // Clear attention state when an expanded panel becomes active.
  // On some systems (e.g. Win), mouse-down activates a panel regardless of
  // its expansion state. However, we don't want to clear draw attention if
  // contents are not visible. In that scenario, if the mouse-down results
  // in a mouse-click, draw attention will be cleared then.
  // See Panel::OnTitlebarClicked().
  if (active && IsDrawingAttention() && !IsMinimized())
    FlashFrame(false);

  if (collection_)
    collection_->OnPanelActiveStateChanged(this);

  // Send extension event about window changing active state.
  extensions::TabsWindowsAPI* tabs_windows_api =
      extensions::TabsWindowsAPI::Get(profile());
  if (tabs_windows_api) {
    tabs_windows_api->windows_event_router()->OnActiveWindowChanged(
        active ? extension_window_controller_.get() : NULL);
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_CHANGED_ACTIVE_STATUS,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}

void Panel::ConfigureAutoResize(content::WebContents* web_contents) {
  if (!auto_resizable_ || !web_contents)
    return;

  // NULL might be returned if the tab has not been added.
  RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  if (!render_view_host)
    return;

  render_view_host->EnableAutoResize(
      min_size_,
      native_panel_->ContentSizeFromWindowSize(max_size_));
}

void Panel::OnWindowSizeAvailable() {
  ConfigureAutoResize(GetWebContents());
}

void Panel::OnTitlebarClicked(panel::ClickModifier modifier) {
  if (collection_)
    collection_->OnPanelTitlebarClicked(this, modifier);

  // Normally the system activates a window when the titlebar is clicked.
  // However, we prevent system activation of minimized panels, thus the
  // activation may not have occurred. Also, some OSes (Windows) will
  // activate a minimized panel on mouse-down regardless of our attempts to
  // prevent system activation. Attention state is not cleared in that case.
  // See Panel::OnActiveStateChanged().
  // Therefore, we ensure activation and clearing of attention state here.
  // These are no-ops if no changes are needed.
  Activate();
  FlashFrame(false);
}

void Panel::OnMinimizeButtonClicked(panel::ClickModifier modifier) {
  if (collection_)
    collection_->OnMinimizeButtonClicked(this, modifier);
}

void Panel::OnRestoreButtonClicked(panel::ClickModifier modifier) {
  if (collection_)
    collection_->OnRestoreButtonClicked(this, modifier);
}

void Panel::OnPanelStartUserResizing() {
  SetAutoResizable(false);
  SetPreviewMode(true);
  max_size_policy_ = CUSTOM_MAX_SIZE;
}

void Panel::OnPanelEndUserResizing() {
  SetPreviewMode(false);
}

bool Panel::ShouldCloseWindow() {
  return true;
}

void Panel::OnWindowClosing() {
  if (GetWebContents()) {
    native_panel_->DetachWebContents(GetWebContents());
    panel_host_->DestroyWebContents();
  }
}

string16 Panel::GetWindowTitle() const {
  content::WebContents* contents = GetWebContents();
  string16 title;

  // |contents| can be NULL during the window's creation.
  if (contents) {
    title = contents->GetTitle();
    FormatTitleForDisplay(&title);
  }

  if (title.empty())
    title = UTF8ToUTF16(app_name());

  return title;
}

// static
void Panel::FormatTitleForDisplay(string16* title) {
  size_t current_index = 0;
  size_t match_index;
  while ((match_index = title->find(L'\n', current_index)) != string16::npos) {
    title->replace(match_index, 1, string16());
    current_index = match_index;
  }
}

gfx::Image Panel::GetCurrentPageIcon() const {
  return panel_host_->GetPageIcon();
}

void Panel::UpdateTitleBar() {
  native_panel_->UpdatePanelTitleBar();
}

void Panel::LoadingStateChanged(bool is_loading) {
  command_updater_.UpdateCommandEnabled(IDC_STOP, is_loading);
  native_panel_->UpdatePanelLoadingAnimations(is_loading);
  UpdateTitleBar();
}

void Panel::WebContentsFocused(content::WebContents* contents) {
  native_panel_->PanelWebContentsFocused(contents);
}

void Panel::MoveByInstantly(const gfx::Vector2d& delta_origin) {
  gfx::Rect bounds = GetBounds();
  bounds.Offset(delta_origin);
  SetPanelBoundsInstantly(bounds);
}

const extensions::Extension* Panel::GetExtension() const {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  if (!extension_service || !extension_service->is_ready())
    return NULL;
  return extension_service->GetExtensionById(extension_id(), false);
}

void Panel::UpdateAppIcon() {
  const extensions::Extension* extension = GetExtension();
  if (!extension)
    return;

  extensions::ImageLoader* loader = extensions::ImageLoader::Get(profile());
  loader->LoadImageAsync(
      extension,
      extension->GetIconResource(extension_misc::EXTENSION_ICON_SMALL,
                                 ExtensionIconSet::MATCH_BIGGER),
      gfx::Size(extension_misc::EXTENSION_ICON_SMALL,
                extension_misc::EXTENSION_ICON_SMALL),
      base::Bind(&Panel::OnImageLoaded,
                 image_loader_ptr_factory_.GetWeakPtr()));
}

void Panel::OnImageLoaded(const gfx::Image& image) {
  if (!image.IsEmpty()) {
    app_icon_ = image;
    native_panel_->UpdatePanelTitleBar();
  }

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PANEL_APP_ICON_LOADED,
      content::Source<Panel>(this),
      content::NotificationService::NoDetails());
}
