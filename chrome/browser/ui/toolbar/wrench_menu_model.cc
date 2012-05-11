// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_model.h"

#include <algorithm>
#include <cmath>

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/global_error_service.h"
#include "chrome/browser/ui/global_error_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/encoding_menu_controller.h"
#include "chrome/browser/upgrade_detector.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/profiling.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/button_menu_item_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#include "chrome/browser/ui/gtk/gtk_util.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/enumerate_modules_model_win.h"
#endif

using content::HostZoomMap;
using content::UserMetricsAction;
using content::WebContents;

////////////////////////////////////////////////////////////////////////////////
// EncodingMenuModel

EncodingMenuModel::EncodingMenuModel(Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
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
    string16& label = it->second;
    if (id == 0) {
      AddSeparator();
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
  WebContents* current_tab = browser_->GetSelectedWebContents();
  if (!current_tab)
    return false;
  EncodingMenuController controller;
  return controller.IsItemChecked(browser_->profile(),
                                  current_tab->GetEncoding(), command_id);
}

bool EncodingMenuModel::IsCommandIdEnabled(int command_id) const {
  bool enabled = browser_->command_updater()->IsCommandEnabled(command_id);
  // Special handling for the contents of the Encoding submenu. On Mac OS,
  // instead of enabling/disabling the top-level menu item, the submenu's
  // contents get disabled, per Apple's HIG.
#if defined(OS_MACOSX)
  enabled &= browser_->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU);
#endif
  return enabled;
}

bool EncodingMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void EncodingMenuModel::ExecuteCommand(int command_id) {
  browser_->ExecuteCommand(command_id);
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
// ToolsMenuModel

ToolsMenuModel::ToolsMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                               Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

ToolsMenuModel::~ToolsMenuModel() {}

void ToolsMenuModel::Build(Browser* browser) {
#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
  AddItemWithStringId(IDC_CREATE_SHORTCUTS, IDS_CREATE_SHORTCUTS);
  AddSeparator();
#endif

  AddItemWithStringId(IDC_MANAGE_EXTENSIONS, IDS_SHOW_EXTENSIONS);
  AddItemWithStringId(IDC_TASK_MANAGER, IDS_TASK_MANAGER);
  AddItemWithStringId(IDC_CLEAR_BROWSING_DATA, IDS_CLEAR_BROWSING_DATA);

  AddSeparator();

#if !defined(OS_CHROMEOS)
  // Show IDC_FEEDBACK in "Tools" menu for non-ChromeOS platforms.
  AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);
  AddSeparator();
#endif

  encoding_menu_model_.reset(new EncodingMenuModel(browser));
  AddSubMenuWithStringId(IDC_ENCODING_MENU, IDS_ENCODING_MENU,
                         encoding_menu_model_.get());
  AddItemWithStringId(IDC_VIEW_SOURCE, IDS_VIEW_SOURCE);
  AddItemWithStringId(IDC_DEV_TOOLS, IDS_DEV_TOOLS);
  AddItemWithStringId(IDC_DEV_TOOLS_CONSOLE, IDS_DEV_TOOLS_CONSOLE);

#if defined(ENABLE_PROFILING) && !defined(NO_TCMALLOC)
  AddSeparator();
  AddCheckItemWithStringId(IDC_PROFILING_ENABLED, IDS_PROFILING_ENABLED);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// WrenchMenuModel

WrenchMenuModel::WrenchMenuModel(ui::AcceleratorProvider* provider,
                                 Browser* browser)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      provider_(provider),
      browser_(browser),
      tabstrip_model_(browser_->tabstrip_model()) {
  Build();
  UpdateZoomControls();

  tabstrip_model_->AddObserver(this);

  registrar_.Add(
      this, content::NOTIFICATION_ZOOM_LEVEL_CHANGED,
      content::Source<HostZoomMap>(
          HostZoomMap::GetForBrowserContext(browser_->profile())));
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                 content::NotificationService::AllSources());
}

WrenchMenuModel::~WrenchMenuModel() {
  if (tabstrip_model_)
    tabstrip_model_->RemoveObserver(this);
}

bool WrenchMenuModel::DoesCommandIdDismissMenu(int command_id) const {
  return command_id != IDC_ZOOM_MINUS && command_id != IDC_ZOOM_PLUS;
}

bool WrenchMenuModel::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_ZOOM_PERCENT_DISPLAY ||
#if defined(OS_MACOSX)
         command_id == IDC_FULLSCREEN ||
#endif
         command_id == IDC_VIEW_BACKGROUND_PAGES ||
         command_id == IDC_UPGRADE_DIALOG ||
         command_id == IDC_SHOW_SYNC_SETUP;
}

string16 WrenchMenuModel::GetLabelForCommandId(int command_id) const {
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
#endif
    case IDC_VIEW_BACKGROUND_PAGES: {
      string16 num_background_pages = base::FormatNumber(
          TaskManager::GetBackgroundPageCount());
      return l10n_util::GetStringFUTF16(IDS_VIEW_BACKGROUND_PAGES,
                                        num_background_pages);
    }
    case IDC_UPGRADE_DIALOG:
      return l10n_util::GetStringUTF16(IDS_UPDATE_NOW);
    case IDC_SHOW_SYNC_SETUP: {
      ProfileSyncService* service =
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(
              browser_->profile()->GetOriginalProfile());
      SyncGlobalError* error = service->sync_global_error();
      if (error && error->HasCustomizedSyncMenuItem())
        return error->MenuItemLabel();
      if (service->HasSyncSetupCompleted()) {
        std::string username = browser_->profile()->GetPrefs()->GetString(
            prefs::kGoogleServicesUsername);
        if (!username.empty()) {
          return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_SYNCED_LABEL,
                                            UTF8ToUTF16(username));
        }
      }
      return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_PRE_SYNCED_LABEL,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
    }
    default:
      NOTREACHED();
      return string16();
  }
}

bool WrenchMenuModel::GetIconForCommandId(int command_id,
                                          SkBitmap* icon) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  switch (command_id) {
    case IDC_UPGRADE_DIALOG: {
      if (UpgradeDetector::GetInstance()->notify_upgrade()) {
        *icon = *rb.GetNativeImageNamed(
            UpgradeDetector::GetInstance()->GetIconResourceID(
                UpgradeDetector::UPGRADE_ICON_TYPE_MENU_ICON)).ToSkBitmap();
        return true;
      }
      return false;
    }
    case IDC_SHOW_SYNC_SETUP: {
      ProfileSyncService* service =
          ProfileSyncServiceFactory::GetInstance()->GetForProfile(
              browser_->profile()->GetOriginalProfile());
      SyncGlobalError* error = service->sync_global_error();
      if (error && error->HasCustomizedSyncMenuItem()) {
        int icon_id = error->MenuItemIconResourceID();
        if (icon_id) {
          *icon = *rb.GetNativeImageNamed(icon_id).ToSkBitmap();
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

void WrenchMenuModel::ExecuteCommand(int command_id) {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  if (command_id == IDC_SHOW_SYNC_SETUP) {
    ProfileSyncService* service =
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(
            browser_->profile()->GetOriginalProfile());
    SyncGlobalError* error = service->sync_global_error();
    if (error && error->HasCustomizedSyncMenuItem()) {
      error->ExecuteMenuItem(browser_);
      return;
    }
  }

  if (command_id == IDC_HELP_PAGE)
    content::RecordAction(UserMetricsAction("ShowHelpTabViaWrenchMenu"));

  browser_->ExecuteCommand(command_id);
}

bool WrenchMenuModel::IsCommandIdChecked(int command_id) const {
  if (command_id == IDC_SHOW_BOOKMARK_BAR) {
    return browser_->profile()->GetPrefs()->GetBoolean(prefs::kShowBookmarkBar);
  } else if (command_id == IDC_PROFILING_ENABLED) {
    return Profiling::BeingProfiled();
  }

  return false;
}

bool WrenchMenuModel::IsCommandIdEnabled(int command_id) const {
  GlobalError* error = GlobalErrorServiceFactory::GetForProfile(
      browser_->profile())->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error)
    return true;

  return browser_->command_updater()->IsCommandEnabled(command_id);
}

bool WrenchMenuModel::IsCommandIdVisible(int command_id) const {
  if (command_id == IDC_UPGRADE_DIALOG) {
    return UpgradeDetector::GetInstance()->notify_upgrade();
  } else if (command_id == IDC_VIEW_INCOMPATIBILITIES) {
#if defined(OS_WIN)
    EnumerateModulesModel* loaded_modules =
        EnumerateModulesModel::GetInstance();
    if (loaded_modules->confirmed_bad_modules_detected() <= 0)
      return false;
    loaded_modules->AcknowledgeConflictNotification();
    return true;
#else
    return false;
#endif
  } else if (command_id == IDC_VIEW_BACKGROUND_PAGES) {
    return TaskManager::GetBackgroundPageCount() > 0;
  }
  return true;
}

bool WrenchMenuModel::GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

void WrenchMenuModel::ActiveTabChanged(TabContentsWrapper* old_contents,
                                       TabContentsWrapper* new_contents,
                                       int index,
                                       bool user_gesture) {
  // The user has switched between tabs and the new tab may have a different
  // zoom setting.
  UpdateZoomControls();
}

void WrenchMenuModel::TabReplacedAt(TabStripModel* tab_strip_model,
                                    TabContentsWrapper* old_contents,
                                    TabContentsWrapper* new_contents,
                                    int index) {
  UpdateZoomControls();
}

void WrenchMenuModel::TabStripModelDeleted() {
  // During views shutdown, the tabstrip model/browser is deleted first, while
  // it is the opposite in gtk land.
  tabstrip_model_->RemoveObserver(this);
  tabstrip_model_ = NULL;
}

void WrenchMenuModel::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_ZOOM_LEVEL_CHANGED:
    case content::NOTIFICATION_NAV_ENTRY_COMMITTED:
      UpdateZoomControls();
      break;
    default:
      NOTREACHED();
  }
}

// For testing.
WrenchMenuModel::WrenchMenuModel()
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      provider_(NULL),
      browser_(NULL),
      tabstrip_model_(NULL) {
}

void WrenchMenuModel::Build() {
  AddItemWithStringId(IDC_NEW_TAB, IDS_NEW_TAB);
  AddItemWithStringId(IDC_NEW_WINDOW, IDS_NEW_WINDOW);
#if defined(OS_CHROMEOS)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession))
    AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);
#else
  AddItemWithStringId(IDC_NEW_INCOGNITO_WINDOW, IDS_NEW_INCOGNITO_WINDOW);
#endif

  bookmark_sub_menu_model_.reset(new BookmarkSubMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_BOOKMARKS_MENU, IDS_BOOKMARKS_MENU,
                         bookmark_sub_menu_model_.get());

  AddSeparator();
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
  // TODO(port): Move to the above.
  CreateCutCopyPaste();
#endif

  AddSeparator();
#if defined(OS_POSIX) && !defined(TOOLKIT_VIEWS)
  // WARNING: See above comment.
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
  // TODO(port): Move to the above.
  CreateZoomFullscreen();
#endif

  AddSeparator();
  AddItemWithStringId(IDC_SAVE_PAGE, IDS_SAVE_PAGE);
  AddItemWithStringId(IDC_FIND, IDS_FIND);
  AddItemWithStringId(IDC_PRINT, IDS_PRINT);

  tools_menu_model_.reset(new ToolsMenuModel(this, browser_));
  AddSubMenuWithStringId(IDC_ZOOM_MENU, IDS_TOOLS_MENU,
                         tools_menu_model_.get());

  AddSeparator();

  AddItemWithStringId(IDC_SHOW_HISTORY, IDS_SHOW_HISTORY);
  AddItemWithStringId(IDC_SHOW_DOWNLOADS, IDS_SHOW_DOWNLOADS);
  AddSeparator();

  if (browser_defaults::kShowSyncSetupMenuItem &&
      browser_->profile()->GetOriginalProfile()->IsSyncAccessible()) {
    const string16 short_product_name =
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
    AddItem(IDC_SHOW_SYNC_SETUP, l10n_util::GetStringFUTF16(
        IDS_SYNC_MENU_PRE_SYNCED_LABEL, short_product_name));
    AddSeparator();
  }

  AddItemWithStringId(IDC_OPTIONS, IDS_SETTINGS);
  AddItem(IDC_ABOUT, l10n_util::GetStringUTF16(IDS_ABOUT));
  string16 num_background_pages = base::FormatNumber(
      TaskManager::GetBackgroundPageCount());
  AddItem(IDC_VIEW_BACKGROUND_PAGES, l10n_util::GetStringFUTF16(
      IDS_VIEW_BACKGROUND_PAGES, num_background_pages));
  if (browser_defaults::kShowUpgradeMenuItem)
    AddItem(IDC_UPGRADE_DIALOG, l10n_util::GetStringUTF16(IDS_UPDATE_NOW));
  AddItem(IDC_VIEW_INCOMPATIBILITIES, l10n_util::GetStringUTF16(
      IDS_VIEW_INCOMPATIBILITIES));

#if defined(OS_WIN)
  SetIcon(GetIndexOfCommandId(IDC_VIEW_INCOMPATIBILITIES),
          *ui::ResourceBundle::GetSharedInstance().
          GetBitmapNamed(IDR_CONFLICT_MENU));
#endif

  AddItemWithStringId(IDC_HELP_PAGE, IDS_HELP_PAGE);
  if (browser_defaults::kShowHelpMenuItemIcon) {
    ui::ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SetIcon(GetIndexOfCommandId(IDC_HELP_PAGE),
            *rb.GetBitmapNamed(IDR_HELP_MENU));
  }

  if (browser_defaults::kShowFeedbackMenuItem)
    AddItemWithStringId(IDC_FEEDBACK, IDS_FEEDBACK);

  AddGlobalErrorMenuItems();

  if (browser_defaults::kShowExitMenuItem) {
    AddSeparator();
    AddItemWithStringId(IDC_EXIT, IDS_EXIT);
  }
}

void WrenchMenuModel::AddGlobalErrorMenuItems() {
  // TODO(sail): Currently we only build the wrench menu once per browser
  // window. This means that if a new error is added after the menu is built
  // it won't show in the existing wrench menu. To fix this we need to some
  // how update the menu if new errors are added.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const GlobalErrorService::GlobalErrorList& errors =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())->errors();
  for (GlobalErrorService::GlobalErrorList::const_iterator
       it = errors.begin(); it != errors.end(); ++it) {
    GlobalError* error = *it;
    if (error->HasMenuItem()) {
      AddItem(error->MenuItemCommandID(), error->MenuItemLabel());
      int icon_id = error->MenuItemIconResourceID();
      if (icon_id) {
        gfx::Image& image = rb.GetImageNamed(icon_id);
        SetIcon(GetIndexOfCommandId(error->MenuItemCommandID()),
                *image.ToSkBitmap());
      }
    }
  }
}

void WrenchMenuModel::CreateCutCopyPaste() {
  // WARNING: views/wrench_menu assumes these items are added in this order. If
  // you change the order you'll need to update wrench_menu as well.
  AddItemWithStringId(IDC_CUT, IDS_CUT);
  AddItemWithStringId(IDC_COPY, IDS_COPY);
  AddItemWithStringId(IDC_PASTE, IDS_PASTE);
}

void WrenchMenuModel::CreateZoomFullscreen() {
  // WARNING: views/wrench_menu assumes these items are added in this order. If
  // you change the order you'll need to update wrench_menu as well.
  AddItemWithStringId(IDC_ZOOM_MINUS, IDS_ZOOM_MINUS);
  AddItemWithStringId(IDC_ZOOM_PLUS, IDS_ZOOM_PLUS);
  AddItemWithStringId(IDC_FULLSCREEN, IDS_FULLSCREEN);
}

void WrenchMenuModel::UpdateZoomControls() {
  bool enable_increment = false;
  bool enable_decrement = false;
  int zoom_percent = 100;
  if (browser_->GetSelectedWebContents()) {
    zoom_percent = browser_->GetSelectedWebContents()->GetZoomPercent(
        &enable_increment, &enable_decrement);
  }
  zoom_label_ = l10n_util::GetStringFUTF16(
      IDS_ZOOM_PERCENT, base::IntToString16(zoom_percent));
}

string16 WrenchMenuModel::GetSyncMenuLabel() const {
  Profile* profile = browser_->profile()->GetOriginalProfile();
  return sync_ui_util::GetSyncMenuLabel(
      ProfileSyncServiceFactory::GetForProfile(profile),
      *SigninManagerFactory::GetForProfile(profile));
}
