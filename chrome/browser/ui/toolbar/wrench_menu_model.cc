// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_model.h"

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.h"
#include "chrome/browser/ui/zoom/zoom_controller.h"
#include "chrome/browser/ui/zoom/zoom_event_manager.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/feature_switch.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

#if defined(OS_WIN)
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "chrome/browser/ui/metro_pin_tab_helper_win.h"
#include "content/public/browser/gpu_data_manager.h"
#endif

#if defined(USE_ASH)
#include "ash/shell.h"
#endif

using base::UserMetricsAction;
using content::HostZoomMap;
using content::WebContents;

namespace {
// Conditionally return the update app menu item title based on upgrade detector
// state.
base::string16 GetUpgradeDialogMenuItemName() {
  if (UpgradeDetector::GetInstance()->is_outdated_install() ||
      UpgradeDetector::GetInstance()->is_outdated_install_no_au()) {
    return l10n_util::GetStringUTF16(IDS_UPGRADE_BUBBLE_MENU_ITEM);
  } else {
    return l10n_util::GetStringUTF16(IDS_UPDATE_NOW);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// EncodingMenuModel

EncodingMenuModel::EncodingMenuModel(Browser* browser)
    : ui::SimpleMenuModel(this),
      browser_(browser) {
  Build();
}

EncodingMenuModel::~EncodingMenuModel() {
}

void EncodingMenuModel::Build() {
  EncodingMenuController::EncodingMenuItemList encoding_menu_items;
  EncodingMenuController encoding_menu_controller;
  encoding_menu_controller.GetEncodingMenuItems(browser_->profile(),
                                                &encoding_menu_items);

  int group_id = 0;
  EncodingMenuController::EncodingMenuItemList::iterator it =
      encoding_menu_items.begin();
  for (; it != encoding_menu_items.end(); ++it) {
    int id = it->first;
    base::string16& label = it->second;
    if (id == 0) {
      AddSeparator(ui::NORMAL_SEPARATOR);
    } else {
      if (id == IDC_ENCODING_AUTO_DETECT) {
        AddCheckItem(id, label);
      } else {
        // Use the id of the first radio command as the id of the group.
        if (group_id <= 0)
          group_id = id;
        AddRadioItem(id, label, group_id);
      }
    }
  }
}

bool EncodingMenuModel::IsCommandIdChecked(int command_id) const {
  WebContents* current_tab =
      browser_->tab_strip_model()->GetActiveWebContents();
  if (!current_tab)
    return false;
  EncodingMenuController controller;
  return controller.IsItemChecked(browser_->profile(),
                                  current_tab->GetEncoding(), command_id);
}

bool EncodingMenuModel::IsCommandIdEnabled(int command_id) const {
  bool enabled = chrome::IsCommandEnabled(browser_, command_id);
  // Special handling for the contents of the Encoding submenu. On Mac OS,
  // instead of enabling/disabling the top-level menu item, the submenu's
  // contents get disabled, per Apple's HIG.
#if defined(OS_MACOSX)
  enabled &= chrome::IsCommandEnabled(browser_, IDC_ENCODING_MENU);
#endif
  return enabled;
}

bool EncodingMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void EncodingMenuModel::ExecuteCommand(int command_id, int event_flags) {
  chrome::ExecuteCommand(browser_, command_id);
}

////////////////////////////////////////////////////////////////////////////////
// ZoomMenuModel

ZoomMenuModel::ZoomMenuModel(ui::SimpleMenuModel::Delegate* delegate)
    : SimpleMenuModel(delegate) {
  Build();
}

ZoomMenuModel::~ZoomMenuModel() {
}

void ZoomMenuModel::Build() {
  AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
  AddItemWithStringId(IDC_ZOOM_NORMAL, IDS_ZOOM_NORMAL);
  AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
}

////////////////////////////////////////////////////////////////////////////////
// HelpMenuModel

#if defined(GOOGLE_CHROME_BUILD)

class WrenchMenuModel::HelpMenuModel : public ui::SimpleMenuModel {
 public:
  HelpMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                Browser* browser)
      : SimpleMenuModel(delegate) {
    Build(browser);
  }
  virtual ~HelpMenuModel() {
  }

 private:
  void Build(Browser* browser) {
    int help_string_id = IDS_HELP_PAGE;
#if defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            chromeos::switches::kDisableGeniusApp)) {
      help_string_id = IDS_GET_HELP;
    }
#endif
    AddItemWithStringId(IDC_HELP_PAGE_VIA_MENU, help_string_id);
    if (browser_defaults::kShowHelpMenuItemIcon) {
      ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
      SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE_VIA_MENU),
              rb.GetNativeImageNamed(IDR_HELP_MENU));
    }

    AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);
  }

  DISALLOW_COPY_AND_ASSIGN(HelpMenuModel);
};

#endif  // defined(GOOGLE_CHROME_BUILD)

////////////////////////////////////////////////////////////////////////////////
// ToolsMenuModel

ToolsMenuModel::ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                               Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ToolsMenuModel::~ToolsMenuModel() {}

void ToolsMenuModel::Build(Browser* browser) {
  bool show_create_shortcuts = true;
#if defined(OS_CHROMEOS) || defined(OS_MACOSX)
  show_create_shortcuts = false;
#elif defined(USE_ASH)
  if (browser->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH)
    show_create_shortcuts = false;
#endif

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableStreamlinedHostedApps)) {
    AddItemWithStringId(IDC_CREATE_HOSTED_APP, IDS_CREATE_HOSTED_APP);
    AddSeparator(ui::NORMAL_SEPARATOR);
  } else if (show_create_shortcuts) {
    AddItemWithStringId(IDC_CREATE_SHORTCUTS, IDS_CREATE_SHORTCUTS);
    AddSeparator(ui::NORMAL_SEPARATOR);
  }

  AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);

  if (chrome::CanOpenTaskManager())
    AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);

  AddItemWithStringId(IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA);

  AddSeparator(ui::NORMAL_SEPARATOR);

  encoding_menu_model_.reset(new EncodingMenuModel(browser));
  AddSubMenuWithStringId(IDC_ENCODING_MENU, IDS_ENCODING_MENU,
                         encoding_menu_model_.get());
  AddItemWithStringId(IDC_VIEW_SOURCE, IDS_VIEW_SOURCE);
  AddItemWithStringId(IDC_DEV_TOOLS, IDS_DEV_TOOLS);
  AddItemWithStringId(IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE);
  AddItemWithStringId(IDC_DEV_TOOLS_DEVICES, IDS_DEV_TOOLS_DEVICES);

#if defined(ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// WrenchMenuModel

WrenchMenuModel::WrenchMenuModel(ui::AcceleratorProvider* provider,
                                 Browser* browser,
                                 bool is_new_menu)
    : ui::SimpleMenuModel(this),
      provider_(provider),
      browser_(browser),
      tab_strip_model_(browser_->tab_strip_model()) {
  Build(is_new_menu);
  UpdateZoomControls();

  content_zoom_subscription_ = content::HostZoomMap::GetForBrowserContext(
      browser->profile())->AddZoomLevelChangedCallback(
          base::Bind(&WrenchMenuModel::OnZoomLevelChanged,
                     base::Unretained(this)));

  browser_zoom_subscription_ = ZoomEventManager::GetForBrowserContext(
      browser->profile())->AddZoomLevelChangedCallback(
          base::Bind(&WrenchMenuModel::OnZoomLevelChanged,
                     base::Unretained(this)));

  tab_strip_model_->AddObserver(this);

  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

WrenchMenuModel::~WrenchMenuModel() {
  if (tab_strip_model_)
    tab_strip_model_->RemoveObserver(this);
}

bool WrenchMenuModel::DoesCommandIdDismissMenu(int command_id) const {
  return command_id != IDC_ZOOM_MINUS && command_id != IDC_ZOOM_PLUS;
}

bool WrenchMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_ZOOM_PERCENT_DISPLAY ||
#if defined(OS_MACOSX)
         command_id == IDC_FULLSCREEN ||
#elif defined(OS_WIN)
         command_id == IDC_PIN_TO_START_SCREEN ||
#endif
         command_id == IDC_UPGRADE_DIALOG ||
         (!switches::IsNewProfileManagement() && command_id == IDC_SHOW_SIGNIN);
}

base::string16 WrenchMenuModel::GetLabelForCommandId(int command_id) const {
  switch (command_id) {
    case IDC_ZOOM_PERCENT_DISPLAY:
      return zoom_label_;
#if defined(OS_MACOSX)
    case IDC_FULLSCREEN: {
      int string_id = IDS_ENTER_FULLSCREEN_MAC;  // Default to Enter.
      // Note: On startup, |window()| may be NULL.
      if (browser_->window() && browser_->window()->IsFullscreen())
        string_id = IDS_EXIT_FULLSCREEN_MAC;
      return l10n_util::GetStringUTF16(string_id);
    }
#elif defined(OS_WIN)
    case IDC_PIN_TO_START_SCREEN: {
      int string_id = IDS_PIN_TO_START_SCREEN;
      WebContents* web_contents =
          browser_->tab_strip_model()->GetActiveWebContents();
      MetroPinTabHelper* tab_helper =
          web_contents ? MetroPinTabHelper::FromWebContents(web_contents)
                       : NULL;
      if (tab_helper && tab_helper->IsPinned())
        string_id = IDS_UNPIN_FROM_START_SCREEN;
      return l10n_util::GetStringUTF16(string_id);
    }
#endif
    case IDC_UPGRADE_DIALOG:
      return GetUpgradeDialogMenuItemName();
    case IDC_SHOW_SIGNIN:
      DCHECK(!switches::IsNewProfileManagement());
      return signin_ui_util::GetSigninMenuLabel(
          browser_->profile()->GetOriginalProfile());
    default:
      NOTREACHED();
      return base::string16();
  }
}

bool WrenchMenuModel::GetIconForCommandId(int command_id,
                                          gfx::Image* icon) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (command_id) {
    case IDC_UPGRADE_DIALOG: {
      if (UpgradeDetector::GetInstance()->notify_upgrade()) {
        *icon = rb.GetNativeImageNamed(
            UpgradeDetector::GetInstance()->GetIconResourceID());
        return true;
      }
      return false;
    }
    case IDC_SHOW_SIGNIN: {
      DCHECK(!switches::IsNewProfileManagement());
      GlobalError* error = signin_ui_util::GetSignedInServiceError(
          browser_->profile()->GetOriginalProfile());
      if (error) {
        int icon_id = error->MenuItemIconResourceID();
        if (icon_id) {
          *icon = rb.GetNativeImageNamed(icon_id);
          return true;
        }
      }
      return false;
    }
    default:
      break;
  }
  return false;
}

void WrenchMenuModel::ExecuteCommand(int command_id, int event_flags) {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  if (!switches::IsNewProfileManagement() && command_id == IDC_SHOW_SIGNIN) {
    // If a custom error message is being shown, handle it.
    GlobalError* error = signin_ui_util::GetSignedInServiceError(
        browser_->profile()->GetOriginalProfile());
    if (error) {
      error->ExecuteMenuItem(browser_);
      return;
    }
  }

  if (command_id == IDC_HELP_PAGE_VIA_MENU)
    content::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

  if (command_id == IDC_FULLSCREEN) {
    // We issue the UMA command here and not in BrowserCommandController or even
    // FullscreenController since we want to be able to distinguish this event
    // and a menu which is under development.
    content::RecordAction(UserMetricsAction("EnterFullScreenWithWrenchMenu"));
  }

  chrome::ExecuteCommand(browser_, command_id);
}

bool WrenchMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  } else if (command_id == IDC_PROFILING_ENABLED) {
    return Profiling::BeingProfiled();
  } else if (command_id == IDC_TOGGLE_REQUEST_TABLET_SITE) {
    return chrome::IsRequestingTabletSite(browser_);
  }

  return false;
}

bool WrenchMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error)
    return true;

  return chrome::IsCommandEnabled(browser_, command_id);
}

bool WrenchMenuModel::IsCommandIdVisible(int command_id) const {
  switch (command_id) {
#if defined(OS_WIN)
    case IDC_VIEW_INCOMPATIBILITIES: {
      EnumerateModulesModel* loaded_modules =
          EnumerateModulesModel::GetInstance();
      if (loaded_modules->confirmed_bad_modules_detected() <= 0)
        return false;
      // We'll leave the wrench adornment on until the user clicks the link.
      if (loaded_modules->modules_to_notify_about() <= 0)
        loaded_modules->AcknowledgeConflictNotification();
      return true;
    }
    case IDC_PIN_TO_START_SCREEN:
      return base::win::IsMetroProcess();
#else
    case IDC_VIEW_INCOMPATIBILITIES:
    case IDC_PIN_TO_START_SCREEN:
      return false;
#endif
    case IDC_UPGRADE_DIALOG:
      return UpgradeDetector::GetInstance()->notify_upgrade();
#if !defined(OS_LINUX) || defined(USE_AURA)
    case IDC_BOOKMARK_PAGE:
      return !chrome::ShouldRemoveBookmarkThisPageUI(browser_->profile());
    case IDC_BOOKMARK_ALL_TABS:
      return !chrome::ShouldRemoveBookmarkOpenPagesUI(browser_->profile());
#endif
    default:
      return true;
  }
}

bool WrenchMenuModel::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

void WrenchMenuModel::ActiveTabChanged(WebContents* old_contents,
                                       WebContents* new_contents,
                                       int index,
                                       int reason) {
  // The user has switched between tabs and the new tab may have a different
  // zoom setting.
  UpdateZoomControls();
}

void WrenchMenuModel::TabReplacedAt(TabStripModel* tab_strip_model,
                                    WebContents* old_contents,
                                    WebContents* new_contents,
                                    int index) {
  UpdateZoomControls();
}

void WrenchMenuModel::TabStripModelDeleted() {
  // During views shutdown, the tabstrip model/browser is deleted first, while
  // it is the opposite in gtk land.
  tab_strip_model_->RemoveObserver(this);
  tab_strip_model_ = NULL;
}

void WrenchMenuModel::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_NAV_ENTRY_COMMITTED);
  UpdateZoomControls();
}

// For testing.
WrenchMenuModel::WrenchMenuModel()
    : ui::SimpleMenuModel(this),
      provider_(NULL),
      browser_(NULL),
      tab_strip_model_(NULL) {
}

bool WrenchMenuModel::ShouldShowNewIncognitoWindowMenuItem() {
  if (browser_->profile()->IsSupervised())
    return false;

  return !browser_->profile()->IsGuestSession();
}

void WrenchMenuModel::Build(bool is_new_menu) {
#if defined(OS_WIN)
  AddItem(IDC_VIEW_INCOMPATIBILITIES,
      l10n_util::GetStringUTF16(IDS_VIEW_INCOMPATIBILITIES));
  EnumerateModulesModel* model =
      EnumerateModulesModel::GetInstance();
  if (model->modules_to_notify_about() > 0 ||
      model->confirmed_bad_modules_detected() > 0)
    AddSeparator(ui::NORMAL_SEPARATOR);
#endif

  if (extensions::FeatureSwitch::extension_action_redesign()->IsEnabled())
    CreateExtensionToolbarOverflowMenu();

  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);

  if (ShouldShowNewIncognitoWindowMenuItem())
    AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);

#if defined(OS_WIN) && !defined(NDEBUG) && defined(USE_ASH)
  if (base::win::GetVersion() < base::win::VERSION_WIN8 &&
      chrome::HOST_DESKTOP_TYPE_NATIVE != chrome::HOST_DESKTOP_TYPE_ASH) {
    AddItemWithStringId(IDC_TOGGLE_ASH_DESKTOP,
                        ash::Shell::HasInstance() ? IDS_CLOSE_ASH_DESKTOP :
                                                    IDS_OPEN_ASH_DESKTOP);
  }
#endif

  bookmark_sub_menu_model_.reset(new BookmarkSubMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_BOOKMARKS_MENU, IDS_BOOKMARKS_MENU,
                         bookmark_sub_menu_model_.get());

  if (!browser_->profile()->IsOffTheRecord()) {
    recent_tabs_sub_menu_model_.reset(new RecentTabsSubMenuModel(provider_,
                                                                 browser_,
                                                                 NULL));
    AddSubMenuWithStringId(IDC_RECENT_TABS_MENU, IDS_RECENT_TABS_MENU,
                           recent_tabs_sub_menu_model_.get());
  }

#if defined(OS_WIN)

#if defined(USE_AURA)
 if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
     content::GpuDataManager::GetInstance()->CanUseGpuBrowserCompositor()) {
    if (browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH) {
      // Metro mode, add the 'Relaunch Chrome in desktop mode'.
      AddSeparator(ui::NORMAL_SEPARATOR);
      AddItemWithStringId(IDC_WIN8_DESKTOP_RESTART, IDS_WIN8_DESKTOP_RESTART);
    } else {
      // In Windows 8 desktop, add the 'Relaunch Chrome in Windows 8 mode'.
      AddSeparator(ui::NORMAL_SEPARATOR);
      AddItemWithStringId(IDC_WIN8_METRO_RESTART, IDS_WIN8_METRO_RESTART);
    }
  }
#else
  if (base::win::IsMetroProcess()) {
    // Metro mode, add the 'Relaunch Chrome in desktop mode'.
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_WIN8_DESKTOP_RESTART, IDS_WIN8_DESKTOP_RESTART);
  } else {
    // In Windows 8 desktop, add the 'Relaunch Chrome in Windows 8 mode'.
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_WIN8_METRO_RESTART, IDS_WIN8_METRO_RESTART);
  }
#endif

#endif

  // Append the full menu including separators. The final separator only gets
  // appended when this is a touch menu - otherwise it would get added twice.
  CreateCutCopyPasteMenu(is_new_menu);

  if (!is_new_menu)
    CreateZoomMenu(is_new_menu);

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller)) {
    AddItemWithStringId(IDC_DISTILL_PAGE, IDS_DISTILL_PAGE);
  }

  AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);
  AddItemWithStringId(IDC_FIND, IDS_FIND);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);

  tools_menu_model_.reset(new ToolsMenuModel(this, browser_));
  // In case of touch this is the last item.
  if (!is_new_menu) {
    AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_TOOLS_MENU,
                           tools_menu_model_.get());
  }

  if (is_new_menu)
    CreateZoomMenu(is_new_menu);
  else
    AddSeparator(ui::NORMAL_SEPARATOR);

  AddItemWithStringId(IDC_SHOW_HISTORY, IDS_SHOW_HISTORY);
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);
  AddSeparator(ui::NORMAL_SEPARATOR);

#if !defined(OS_CHROMEOS)
  if (!switches::IsNewProfileManagement()) {
    // No "Sign in to Chromium..." menu item on ChromeOS.
    SigninManager* signin = SigninManagerFactory::GetForProfile(
        browser_->profile()->GetOriginalProfile());
    if (signin && signin->IsSigninAllowed()) {
      const base::string16 short_product_name =
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
      AddItem(IDC_SHOW_SYNC_SETUP, l10n_util::GetStringFUTF16(
          IDS_SYNC_MENU_PRE_SYNCED_LABEL, short_product_name));
      AddSeparator(ui::NORMAL_SEPARATOR);
    }
  }
#endif

  AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);

// On ChromeOS we don't want the about menu option.
#if !defined(OS_CHROMEOS)
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
#endif

#if defined(GOOGLE_CHROME_BUILD)
  help_menu_model_.reset(new HelpMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_HELP_MENU, IDS_HELP_MENU,
                         help_menu_model_.get());
#endif

#if defined(OS_CHROMEOS)
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kEnableRequestTabletSite))
    AddCheckItemWithStringId(IDC_TOGGLE_REQUEST_TABLET_SITE,
                             IDS_TOGGLE_REQUEST_TABLET_SITE);
#endif

  if (browser_defaults::kShowUpgradeMenuItem)
    AddItem(IDC_UPGRADE_DIALOG, GetUpgradeDialogMenuItemName());

#if defined(OS_WIN)
  SetIcon(GetIndexOfCommandId(IDC_VIEW_INCOMPATIBILITIES),
          ui::ResourceBundle::GetSharedInstance().
              GetNativeImageNamed(IDR_INPUT_ALERT_MENU));
#endif

  AddGlobalErrorMenuItems();

  if (is_new_menu) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_MORE_TOOLS_MENU,
                           tools_menu_model_.get());
  }

  bool show_exit_menu = browser_defaults::kShowExitMenuItem;
#if defined(OS_WIN)
  if (browser_->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH)
    show_exit_menu = false;
#endif

  if (show_exit_menu) {
    AddSeparator(ui::NORMAL_SEPARATOR);
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  }

  RemoveTrailingSeparators();
}

void WrenchMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the wrench menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing wrench menu. To fix this we need to some
  // how update the menu if new errors are added.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  // GetSignedInServiceErrors() can modify the global error list, so call it
  // before iterating through that list below.
  std::vector<GlobalError*> signin_errors;
  if (!switches::IsNewProfileManagement()) {
      signin_errors = signin_ui_util::GetSignedInServiceErrors(
          browser_->profile()->GetOriginalProfile());
  }
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())->errors();
  for (GlobalErrorService::GlobalErrorList::const_iterator
       it = errors.begin(); it != errors.end(); ++it) {
    GlobalError* error = *it;
    DCHECK(error);
    if (error->HasMenuItem()) {
#if !defined(OS_CHROMEOS)
      // Don't add a signin error if it's already being displayed elsewhere.
      if (std::find(signin_errors.begin(), signin_errors.end(), error) !=
          signin_errors.end()) {
        MenuModel* model = this;
        int index = 0;
        if (MenuModel::GetModelAndIndexForCommandId(
                IDC_SHOW_SIGNIN, &model, &index)) {
          continue;
        }
      }
#endif

      AddItem(error->MenuItemCommandID(), error->MenuItemLabel());
      int icon_id = error->MenuItemIconResourceID();
      if (icon_id) {
        const gfx::Image& image = rb.GetNativeImageNamed(icon_id);
        SetIcon(GetIndexOfCommandId(error->MenuItemCommandID()),
                image);
      }
    }
  }
}

void WrenchMenuModel::CreateExtensionToolbarOverflowMenu() {
#if defined(TOOLKIT_VIEWS)
  AddItem(IDC_EXTENSIONS_OVERFLOW_MENU, base::string16());
  // TODO(devlin): Add the separator only if there are > 0 icons to show.
  AddSeparator(ui::UPPER_SEPARATOR);
#endif  // defined(TOOLKIT_VIEWS)
}

void WrenchMenuModel::CreateCutCopyPasteMenu(bool new_menu) {
  AddSeparator(new_menu ? ui::LOWER_SEPARATOR : ui::NORMAL_SEPARATOR);

#if defined(OS_POSIX) && !defined(TOOLKIT_VIEWS)
  // WARNING: Mac does not use the ButtonMenuItemModel, but instead defines the
  // layout for this menu item in Toolbar.xib. It does, however, use the
  // command_id value from AddButtonItem() to identify this special item.
  edit_menu_item_model_.reset(new ui::ButtonMenuItemModel(IDS_EDIT, this));
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_CUT, IDS_CUT);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_COPY, IDS_COPY);
  edit_menu_item_model_->AddGroupItemWithStringId(IDC_PASTE, IDS_PASTE);
  AddButtonItem(IDC_EDIT_MENU, edit_menu_item_model_.get());
#else
  // WARNING: views/wrench_menu assumes these items are added in this order. If
  // you change the order you'll need to update wrench_menu as well.
  AddItemWithStringId(IDC_CUT, IDS_CUT);
  AddItemWithStringId(IDC_COPY, IDS_COPY);
  AddItemWithStringId(IDC_PASTE, IDS_PASTE);
#endif

  if (new_menu)
    AddSeparator(ui::UPPER_SEPARATOR);
}

void WrenchMenuModel::CreateZoomMenu(bool new_menu) {
  // This menu needs to be enclosed by separators.
  AddSeparator(new_menu ? ui::LOWER_SEPARATOR : ui::NORMAL_SEPARATOR);

#if defined(OS_POSIX) && !defined(TOOLKIT_VIEWS)
  // WARNING: Mac does not use the ButtonMenuItemModel, but instead defines the
  // layout for this menu item in Toolbar.xib. It does, however, use the
  // command_id value from AddButtonItem() to identify this special item.
  zoom_menu_item_model_.reset(
      new ui::ButtonMenuItemModel(IDS_ZOOM_MENU, this));
  zoom_menu_item_model_->AddGroupItemWithStringId(
      IDC_ZOOM_MINUS, IDS_ZOOM_MINUS2);
  zoom_menu_item_model_->AddButtonLabel(IDC_ZOOM_PERCENT_DISPLAY,
                                        IDS_ZOOM_PLUS2);
  zoom_menu_item_model_->AddGroupItemWithStringId(
      IDC_ZOOM_PLUS, IDS_ZOOM_PLUS2);
  zoom_menu_item_model_->AddSpace();
  zoom_menu_item_model_->AddItemWithImage(
      IDC_FULLSCREEN, IDR_FULLSCREEN_MENU_BUTTON);
  AddButtonItem(IDC_ZOOM_MENU, zoom_menu_item_model_.get());
#else
  // WARNING: views/wrench_menu assumes these items are added in this order. If
  // you change the order you'll need to update wrench_menu as well.
  AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
  AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
  AddItemWithStringId(IDC_FULLSCREEN, IDS_FULLSCREEN);
#endif

  AddSeparator(new_menu ? ui::UPPER_SEPARATOR : ui::NORMAL_SEPARATOR);
}

void WrenchMenuModel::UpdateZoomControls() {
  int zoom_percent = 100;
  if (browser_->tab_strip_model()->GetActiveWebContents()) {
    zoom_percent = ZoomController::FromWebContents(
                       browser_->tab_strip_model()->GetActiveWebContents())
                       ->GetZoomPercent();
  }
  zoom_label_ = l10n_util::GetStringFUTF16(
      IDS_ZOOM_PERCENT, base::IntToString16(zoom_percent));
}

void WrenchMenuModel::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  UpdateZoomControls();
}
