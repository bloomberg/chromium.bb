// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_tab.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/manifest_handlers/icons_handler.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "url/gurl.h"

// Size of extension icon in top left of dialog.
const int kIconSize = 64;

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
};

base::string16 LaunchOptionsComboboxModel::GetItemAt(int index) {
  return launch_type_messages_[index];
};

AppInfoSummaryTab::AppInfoSummaryTab(gfx::NativeWindow parent_window,
                                     Profile* profile,
                                     const extensions::Extension* app,
                                     const base::Closure& close_callback)
    : AppInfoTab(parent_window, profile, app, close_callback),
      app_icon_(NULL),
      launch_options_combobox_(NULL),
      weak_ptr_factory_(this) {
  // Create UI elements.
  views::Label* app_name_label =
      new views::Label(base::UTF8ToUTF16(app_->name()));
  app_name_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* app_description_label =
      new views::Label(base::UTF8ToUTF16(app_->description()));
  app_description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  views::Label* app_version_label =
      new views::Label(base::UTF8ToUTF16(app_->VersionString()));
  app_version_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  app_icon_ = new views::ImageView();
  app_icon_->SetImageSize(gfx::Size(kIconSize, kIconSize));
  LoadAppImageAsync();

  // Create the layout.
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Create a Header column set with app icon and information.
  static const int kHeaderColumnSetId = 0;
  views::ColumnSet* header_column_set =
      layout->AddColumnSet(kHeaderColumnSetId);
  header_column_set->AddColumn(views::GridLayout::FILL,
                               views::GridLayout::CENTER,
                               0,
                               views::GridLayout::FIXED,
                               kIconSize,
                               0);
  header_column_set->AddPaddingColumn(0,
                                      views::kRelatedControlHorizontalSpacing);
  header_column_set->AddColumn(views::GridLayout::FILL,
                               views::GridLayout::CENTER,
                               100.0f,
                               views::GridLayout::FIXED,
                               0,
                               0);

  // Create a main column set for the rest of the dialog content.
  static const int kMainColumnSetId = 1;
  views::ColumnSet* main_column_set = layout->AddColumnSet(kMainColumnSetId);
  main_column_set->AddColumn(views::GridLayout::FILL,
                             views::GridLayout::LEADING,
                             100.0f,
                             views::GridLayout::FIXED,
                             0,
                             0);

  // The app icon takes up 3 rows.
  layout->StartRow(0, kHeaderColumnSetId);
  layout->AddView(app_icon_, 1, 3);

  // The app information fills up the right side of the icon.
  layout->AddView(app_name_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_version_label);

  layout->StartRow(0, kHeaderColumnSetId);
  layout->SkipColumns(1);
  layout->AddView(app_description_label);

  // Add a link to the web store for apps that have it.
  if (CanShowAppInWebStore()) {
    view_in_store_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_WEB_STORE_LINK));
    view_in_store_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    view_in_store_link_->set_listener(this);

    layout->StartRow(0, kMainColumnSetId);
    layout->AddView(view_in_store_link_);
  }

  // Add hosted app launch options for non-platform apps.
  if (CanSetLaunchType()) {
    launch_options_combobox_model_.reset(new LaunchOptionsComboboxModel());
    launch_options_combobox_ =
        new views::Combobox(launch_options_combobox_model_.get());

    launch_options_combobox_->set_listener(this);
    launch_options_combobox_->SetSelectedIndex(
        launch_options_combobox_model_->GetIndexForLaunchType(GetLaunchType()));

    layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
    layout->StartRow(0, kMainColumnSetId);
    layout->AddView(launch_options_combobox_);
  }
}

AppInfoSummaryTab::~AppInfoSummaryTab() {
  // Destroy view children before their models.
  RemoveAllChildViews(true);
}

void AppInfoSummaryTab::OnPerformAction(views::Combobox* combobox) {
  if (combobox == launch_options_combobox_) {
    SetLaunchType(launch_options_combobox_model_->GetLaunchTypeAtIndex(
        launch_options_combobox_->selected_index()));
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryTab::LinkClicked(views::Link* source, int event_flags) {
  if (source == view_in_store_link_) {
    ShowAppInWebStore();
  } else {
    NOTREACHED();
  }
}

void AppInfoSummaryTab::LoadAppImageAsync() {
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
      base::Bind(&AppInfoSummaryTab::OnAppImageLoaded, AsWeakPtr()));
}

void AppInfoSummaryTab::OnAppImageLoaded(const gfx::Image& image) {
  const SkBitmap* bitmap;
  if (image.IsEmpty()) {
    bitmap = &extensions::IconsInfo::GetDefaultAppIcon()
                  .GetRepresentation(gfx::ImageSkia::GetMaxSupportedScale())
                  .sk_bitmap();
  } else {
    bitmap = image.ToSkBitmap();
  }

  app_icon_->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(*bitmap));
}

extensions::LaunchType AppInfoSummaryTab::GetLaunchType() const {
  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile_),
                                   app_);
}

void AppInfoSummaryTab::SetLaunchType(extensions::LaunchType launch_type) {
  DCHECK(CanSetLaunchType());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::SetLaunchType(service, app_->id(), launch_type);
}

bool AppInfoSummaryTab::CanSetLaunchType() {
  // V2 apps don't have a launch type.
  return !app_->is_platform_app();
}

bool AppInfoSummaryTab::CanShowAppInWebStore() const {
  return app_->from_webstore();
}

void AppInfoSummaryTab::ShowAppInWebStore() const {
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
