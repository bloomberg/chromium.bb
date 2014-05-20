// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_tab.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "grit/generated_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

namespace {

// A small summary panel with the app's name, icon, version, and a link to the
// web store (if they exist).
class AppInfoSummaryPanel : public views::View,
                            public views::LinkListener,
                            public base::SupportsWeakPtr<AppInfoSummaryPanel> {
 public:
  AppInfoSummaryPanel(Profile* profile, const extensions::Extension* app);
  virtual ~AppInfoSummaryPanel();

 private:
  void CreateControls();
  void LayoutAppNameAndVersionInto(views::View* parent_view);
  void LayoutViews();

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // Load the app icon asynchronously. For the response, check OnAppImageLoaded.
  void LoadAppImageAsync();
  // Called when the app's icon is loaded.
  void OnAppImageLoaded(const gfx::Image& image);

  // Opens the app in the web store. Must only be called if
  // CanShowAppInWebStore() returns true.
  void ShowAppInWebStore() const;
  bool CanShowAppInWebStore() const;

  views::ImageView* app_icon_;
  views::Label* app_name_label_;
  views::Label* app_version_label_;
  views::Link* view_in_store_link_;

  Profile* profile_;
  const extensions::Extension* app_;

  base::WeakPtrFactory<AppInfoSummaryPanel> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppInfoSummaryPanel);
};

AppInfoSummaryPanel::AppInfoSummaryPanel(Profile* profile,
                                         const extensions::Extension* app)
    : app_icon_(NULL),
      app_name_label_(NULL),
      app_version_label_(NULL),
      view_in_store_link_(NULL),
      profile_(profile),
      app_(app),
      weak_ptr_factory_(this) {
  CreateControls();

  // Layout elements.
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0,
                           0,
                           views::kRelatedControlHorizontalSpacing));

  AddChildView(app_icon_);

  if (!app_version_label_ && !view_in_store_link_) {
    // If there's no link to the webstore _and_ no version, allow the app's name
    // to take up multiple lines.
    app_name_label_->SetMultiLine(true);
    AddChildView(app_name_label_);
  } else {
    // Create a vertical container to store the app's name and info.
    views::View* vertical_container = new views::View();
    views::BoxLayout* vertical_container_layout =
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
    vertical_container_layout->set_main_axis_alignment(
        views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
    vertical_container->SetLayoutManager(vertical_container_layout);

    if (app_version_label_ && view_in_store_link_) {
      // First line: title and version, Second line: web store link.
      LayoutAppNameAndVersionInto(vertical_container);
      vertical_container->AddChildView(view_in_store_link_);
    } else {
      // Put the title on the first line, and whatever other information we have
      // on the second line.
      vertical_container->AddChildView(app_name_label_);

      if (app_version_label_) {
        vertical_container->AddChildView(app_version_label_);
      } else if (view_in_store_link_) {
        vertical_container->AddChildView(view_in_store_link_);
      }
    }

    AddChildView(vertical_container);
  }
}

AppInfoSummaryPanel::~AppInfoSummaryPanel() {
}

void AppInfoSummaryPanel::CreateControls() {
  app_name_label_ =
      new views::Label(base::UTF8ToUTF16(app_->name()),
                       ui::ResourceBundle::GetSharedInstance().GetFontList(
                           ui::ResourceBundle::BoldFont));
  app_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  // The version number doesn't make sense for bookmarked apps.
  if (!app_->from_bookmark()) {
    app_version_label_ =
        new views::Label(base::UTF8ToUTF16(app_->VersionString()));
    app_version_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  app_icon_ = new views::ImageView();
  app_icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  LoadAppImageAsync();

  if (CanShowAppInWebStore()) {
    view_in_store_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_WEB_STORE_LINK));
    view_in_store_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view_in_store_link_->set_listener(this);
  }
}

void AppInfoSummaryPanel::LayoutAppNameAndVersionInto(
    views::View* parent_view) {
  views::View* view = new views::View();
  // We need a horizontal BoxLayout here to ensure that the GridLayout does
  // not stretch beyond the size of its content.
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  views::View* container_view = new views::View();
  view->AddChildView(container_view);
  views::GridLayout* layout = new views::GridLayout(container_view);
  container_view->SetLayoutManager(layout);

  static const int kColumnId = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnId);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        1,  // Stretch the title to as wide as needed
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, views::kRelatedControlSmallHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::LEADING,
                        0,  // Do not stretch the version
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(1, kColumnId);
  layout->AddView(app_name_label_);
  if (app_version_label_)
    layout->AddView(app_version_label_);

  parent_view->AddChildView(view);
}

void AppInfoSummaryPanel::LinkClicked(views::Link* source, int event_flags) {
  if (source == view_in_store_link_) {
    ShowAppInWebStore();
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryPanel::LoadAppImageAsync() {
  extensions::ExtensionResource image = extensions::IconsInfo::GetIconResource(
      app_,
      extension_misc::EXTENSION_ICON_LARGE,
      ExtensionIconSet::MATCH_BIGGER);
  int pixel_size =
      static_cast<int>(kIconSize * gfx::ImageSkia::GetMaxSupportedScale());
  extensions::ImageLoader::Get(profile_)->LoadImageAsync(
      app_,
      image,
      gfx::Size(pixel_size, pixel_size),
      base::Bind(&AppInfoSummaryPanel::OnAppImageLoaded, AsWeakPtr()));
}

void AppInfoSummaryPanel::OnAppImageLoaded(const gfx::Image& image) {
  const SkBitmap* bitmap;
  if (image.IsEmpty()) {
    bitmap = &extensions::util::GetDefaultAppIcon()
                  .GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
                  .sk_bitmap();
  } else {
    bitmap = image.ToSkBitmap();
  }

  app_icon_->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

void AppInfoSummaryPanel::ShowAppInWebStore() const {
  DCHECK(CanShowAppInWebStore());
  const GURL url = extensions::ManifestURL::GetDetailsURL(app_);
  DCHECK_NE(url, GURL::EmptyGURL());
  chrome::NavigateParams params(
      profile_,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                extension_urls::kLaunchSourceAppListInfoDialog),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

bool AppInfoSummaryPanel::CanShowAppInWebStore() const {
  return app_->from_webstore();
}

}  // namespace

// A model for a combobox selecting the launch options for a hosted app.
// Displays different options depending on the host OS.
class LaunchOptionsComboboxModel : public ui::ComboboxModel {
 public:
  LaunchOptionsComboboxModel();
  virtual ~LaunchOptionsComboboxModel();

  extensions::LaunchType GetLaunchTypeAtIndex(int index) const;
  int GetIndexForLaunchType(extensions::LaunchType launch_type) const;

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;

 private:
  // A list of the launch types available in the combobox, in order.
  std::vector<extensions::LaunchType> launch_types_;

  // A list of the messages to display in the combobox, in order. The indexes in
  // this list correspond to the indexes in launch_types_.
  std::vector<base::string16> launch_type_messages_;
};

LaunchOptionsComboboxModel::LaunchOptionsComboboxModel() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableStreamlinedHostedApps)) {
    // Streamlined hosted apps can only toggle between LAUNCH_TYPE_WINDOW and
    // LAUNCH_TYPE_REGULAR.
    // TODO(sashab): Use a checkbox for this choice instead of combobox.
    launch_types_.push_back(extensions::LAUNCH_TYPE_REGULAR);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_TAB));

    // Although LAUNCH_TYPE_WINDOW doesn't work on Mac, the streamlined hosted
    // apps flag isn't available on Mac, so we must be on a non-Mac OS.
    launch_types_.push_back(extensions::LAUNCH_TYPE_WINDOW);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));
  } else {
    launch_types_.push_back(extensions::LAUNCH_TYPE_REGULAR);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_REGULAR));

    launch_types_.push_back(extensions::LAUNCH_TYPE_PINNED);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_PINNED));

#if defined(OS_MACOSX)
    // Mac does not support standalone web app browser windows or maximize.
    launch_types_.push_back(extensions::LAUNCH_TYPE_FULLSCREEN);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_FULLSCREEN));
#else
    launch_types_.push_back(extensions::LAUNCH_TYPE_WINDOW);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_WINDOW));

    // Even though the launch type is Full Screen, it is more accurately
    // described as Maximized in non-Mac OSs.
    launch_types_.push_back(extensions::LAUNCH_TYPE_FULLSCREEN);
    launch_type_messages_.push_back(
        l10n_util::GetStringUTF16(IDS_APP_CONTEXT_MENU_OPEN_MAXIMIZED));
#endif
  }
}

LaunchOptionsComboboxModel::~LaunchOptionsComboboxModel() {}

extensions::LaunchType LaunchOptionsComboboxModel::GetLaunchTypeAtIndex(
    int index) const {
  return launch_types_[index];
}

int LaunchOptionsComboboxModel::GetIndexForLaunchType(
    extensions::LaunchType launch_type) const {
  for (size_t i = 0; i < launch_types_.size(); i++) {
    if (launch_types_[i] == launch_type) {
      return i;
    }
  }
  // If the requested launch type is not available, just select the first one.
  LOG(WARNING) << "Unavailable launch type " << launch_type << " selected.";
  return 0;
}

int LaunchOptionsComboboxModel::GetItemCount() const {
  return launch_types_.size();
}

base::string16 LaunchOptionsComboboxModel::GetItemAt(int index) {
  return launch_type_messages_[index];
}

AppInfoSummaryTab::AppInfoSummaryTab(gfx::NativeWindow parent_window,
                                     Profile* profile,
                                     const extensions::Extension* app,
                                     const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback),
      app_summary_panel_(NULL),
      app_description_label_(NULL),
      create_shortcuts_button_(NULL),
      uninstall_button_(NULL),
      launch_options_combobox_(NULL) {
  // Create UI elements.
  app_summary_panel_ = new AppInfoSummaryPanel(profile_, app_);
  CreateDescriptionControl();
  CreateLaunchOptionControl();
  CreateButtons();

  // Layout elements.
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           views::kButtonHEdgeMarginNew,
                           views::kPanelVertMargin,
                           views::kUnrelatedControlVerticalSpacing));

  AddChildView(app_summary_panel_);

  if (app_description_label_)
    AddChildView(app_description_label_);

  if (launch_options_combobox_)
    AddChildView(launch_options_combobox_);

  LayoutButtons();
}

AppInfoSummaryTab::~AppInfoSummaryTab() {
  // Destroy view children before their models.
  RemoveAllChildViews(true);
}

void AppInfoSummaryTab::CreateDescriptionControl() {
  if (!app_->description().empty()) {
    const size_t kMaxLength = 400;

    base::string16 text = base::UTF8ToUTF16(app_->description());
    if (text.length() > kMaxLength) {
      text = text.substr(0, kMaxLength);
      text += base::ASCIIToUTF16(" ... ");
    }

    app_description_label_ = new views::Label(text);
    app_description_label_->SetMultiLine(true);
    app_description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
}

void AppInfoSummaryTab::CreateLaunchOptionControl() {
  if (CanSetLaunchType()) {
    launch_options_combobox_model_.reset(new LaunchOptionsComboboxModel());
    launch_options_combobox_ =
        new views::Combobox(launch_options_combobox_model_.get());

    launch_options_combobox_->set_listener(this);
    launch_options_combobox_->SetSelectedIndex(
        launch_options_combobox_model_->GetIndexForLaunchType(GetLaunchType()));
  }
}

void AppInfoSummaryTab::CreateButtons() {
  if (CanCreateShortcuts()) {
    create_shortcuts_button_ = new views::LabelButton(
        this,
        l10n_util::GetStringUTF16(
            IDS_APPLICATION_INFO_CREATE_SHORTCUTS_BUTTON_TEXT));
    create_shortcuts_button_->SetStyle(views::Button::STYLE_BUTTON);
  }

  if (CanUninstallApp()) {
    uninstall_button_ = new views::LabelButton(
        this,
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_UNINSTALL_BUTTON_TEXT));
    uninstall_button_->SetStyle(views::Button::STYLE_BUTTON);
  }
}

void AppInfoSummaryTab::LayoutButtons() {
  views::View* app_buttons = new views::View();
  app_buttons->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           0,
                           0,
                           views::kRelatedControlVerticalSpacing));

  if (create_shortcuts_button_)
    app_buttons->AddChildView(create_shortcuts_button_);

  if (uninstall_button_)
    app_buttons->AddChildView(uninstall_button_);

  AddChildView(app_buttons);
}

void AppInfoSummaryTab::OnPerformAction(views::Combobox* combobox) {
  if (combobox == launch_options_combobox_) {
    SetLaunchType(launch_options_combobox_model_->GetLaunchTypeAtIndex(
        launch_options_combobox_->selected_index()));
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryTab::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == uninstall_button_) {
    UninstallApp();
  } else if (sender == create_shortcuts_button_) {
    CreateShortcuts();
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryTab::ExtensionUninstallAccepted() {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  service->UninstallExtension(app_->id(), false, NULL);

  // Close the App Info dialog as well (which will free the dialog too).
  GetWidget()->Close();
}

void AppInfoSummaryTab::ExtensionUninstallCanceled() {
  extension_uninstall_dialog_.reset();
}

extensions::LaunchType AppInfoSummaryTab::GetLaunchType() const {
  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile_),
                                   app_);
}

void AppInfoSummaryTab::SetLaunchType(
    extensions::LaunchType launch_type) const {
  DCHECK(CanSetLaunchType());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::SetLaunchType(service, app_->id(), launch_type);
}

bool AppInfoSummaryTab::CanSetLaunchType() const {
  // V2 apps don't have a launch type.
  return !app_->is_platform_app();
}

void AppInfoSummaryTab::UninstallApp() {
  DCHECK(CanUninstallApp());
  extension_uninstall_dialog_.reset(
      ExtensionUninstallDialog::Create(profile_, NULL, this));
  extension_uninstall_dialog_->ConfirmUninstall(app_);
}

bool AppInfoSummaryTab::CanUninstallApp() const {
  return extensions::ExtensionSystem::Get(profile_)
      ->management_policy()
      ->UserMayModifySettings(app_, NULL);
}

void AppInfoSummaryTab::CreateShortcuts() {
  DCHECK(CanCreateShortcuts());
  chrome::ShowCreateChromeAppShortcutsDialog(
      parent_window_, profile_, app_, base::Callback<void(bool)>());
}

bool AppInfoSummaryTab::CanCreateShortcuts() const {
  // ChromeOS can pin apps to the app launcher, but can't create shortcuts.
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}
