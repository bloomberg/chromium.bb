// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"

#include <vector>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

LaunchOptionsComboboxModel::~LaunchOptionsComboboxModel() {
}

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

AppInfoSummaryPanel::AppInfoSummaryPanel(Profile* profile,
                                         const extensions::Extension* app)
    : AppInfoPanel(profile, app),
      description_heading_(NULL),
      description_label_(NULL),
      details_heading_(NULL),
      version_title_(NULL),
      version_value_(NULL),
      installed_time_title_(NULL),
      installed_time_value_(NULL),
      last_run_time_title_(NULL),
      last_run_time_value_(NULL),
      launch_options_combobox_(NULL) {
  // Create UI elements.
  CreateDescriptionControl();
  CreateDetailsControl();
  CreateLaunchOptionControl();

  // Layout elements.
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical,
                           0,
                           0,
                           views::kUnrelatedControlVerticalSpacing));

  LayoutDescriptionControl();
  LayoutDetailsControl();

  if (launch_options_combobox_)
    AddChildView(launch_options_combobox_);
}

AppInfoSummaryPanel::~AppInfoSummaryPanel() {
  // Destroy view children before their models.
  RemoveAllChildViews(true);
}

void AppInfoSummaryPanel::CreateDescriptionControl() {
  if (!app_->description().empty()) {
    const size_t kMaxLength = 400;

    base::string16 text = base::UTF8ToUTF16(app_->description());
    if (text.length() > kMaxLength) {
      text = text.substr(0, kMaxLength);
      text += base::ASCIIToUTF16(" ... ");
    }

    description_heading_ = CreateHeading(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_DESCRIPTION_TITLE));
    description_label_ = new views::Label(text);
    description_label_->SetMultiLine(true);
    description_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }
}

void AppInfoSummaryPanel::CreateDetailsControl() {
  // The version doesn't make sense for bookmark apps.
  if (!app_->from_bookmark()) {
    // Display 'Version: Built-in' for component apps.
    base::string16 version_str = base::ASCIIToUTF16(app_->VersionString());
    if (app_->location() == extensions::Manifest::COMPONENT)
      version_str = l10n_util::GetStringUTF16(
          IDS_APPLICATION_INFO_VERSION_BUILT_IN_LABEL);

    version_title_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_VERSION_LABEL));
    version_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    version_value_ = new views::Label(version_str);
    version_value_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  // The install date doesn't make sense for component apps.
  if (app_->location() != extensions::Manifest::COMPONENT) {
    installed_time_title_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_INSTALLED_LABEL));
    installed_time_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    installed_time_value_ =
        new views::Label(base::TimeFormatShortDate(GetInstalledTime()));
    installed_time_value_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  // The last run time is currently incorrect for component and hosted apps,
  // since it is not updated when they are accessed outside of their shortcuts.
  // TODO(sashab): Update the run time for these correctly: crbug.com/398716
  if (app_->location() != extensions::Manifest::COMPONENT &&
      !app_->is_hosted_app()) {
    last_run_time_title_ = new views::Label(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_LAST_RUN_LABEL));
    last_run_time_title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Display 'Never' if the app has never been run.
    base::string16 last_run_value_str =
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_LAST_RUN_NEVER_LABEL);
    if (GetLastLaunchedTime() != base::Time())
      last_run_value_str = base::TimeFormatShortDate(GetLastLaunchedTime());

    last_run_time_value_ = new views::Label(last_run_value_str);
    last_run_time_value_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  }

  // Only generate the heading if we have at least one field to display.
  if (version_title_ || installed_time_title_ || last_run_time_title_) {
    details_heading_ = CreateHeading(
        l10n_util::GetStringUTF16(IDS_APPLICATION_INFO_DETAILS_TITLE));
  }
}

void AppInfoSummaryPanel::CreateLaunchOptionControl() {
  if (CanSetLaunchType()) {
    launch_options_combobox_model_.reset(new LaunchOptionsComboboxModel());
    launch_options_combobox_ =
        new views::Combobox(launch_options_combobox_model_.get());

    launch_options_combobox_->set_listener(this);
    launch_options_combobox_->SetSelectedIndex(
        launch_options_combobox_model_->GetIndexForLaunchType(GetLaunchType()));
  }
}

void AppInfoSummaryPanel::LayoutDescriptionControl() {
  if (description_label_) {
    DCHECK(description_heading_);
    views::View* vertical_stack = CreateVerticalStack();
    vertical_stack->AddChildView(description_heading_);
    vertical_stack->AddChildView(description_label_);
    AddChildView(vertical_stack);
  }
}

void AppInfoSummaryPanel::LayoutDetailsControl() {
  if (details_heading_) {
    views::View* details_stack =
        CreateVerticalStack(views::kRelatedControlSmallVerticalSpacing);

    if (version_title_ && version_value_) {
      details_stack->AddChildView(
          CreateKeyValueField(version_title_, version_value_));
    }

    if (installed_time_title_ && installed_time_value_) {
      details_stack->AddChildView(
          CreateKeyValueField(installed_time_title_, installed_time_value_));
    }

    if (last_run_time_title_ && last_run_time_value_) {
      details_stack->AddChildView(
          CreateKeyValueField(last_run_time_title_, last_run_time_value_));
    }

    views::View* vertical_stack = CreateVerticalStack();
    vertical_stack->AddChildView(details_heading_);
    vertical_stack->AddChildView(details_stack);
    AddChildView(vertical_stack);
  }
}

void AppInfoSummaryPanel::OnPerformAction(views::Combobox* combobox) {
  if (combobox == launch_options_combobox_) {
    SetLaunchType(launch_options_combobox_model_->GetLaunchTypeAtIndex(
        launch_options_combobox_->selected_index()));
  } else {
    NOTREACHED();
  }
}

base::Time AppInfoSummaryPanel::GetInstalledTime() const {
  return extensions::ExtensionPrefs::Get(profile_)->GetInstallTime(app_->id());
}

base::Time AppInfoSummaryPanel::GetLastLaunchedTime() const {
  return extensions::ExtensionPrefs::Get(profile_)
      ->GetLastLaunchTime(app_->id());
}

extensions::LaunchType AppInfoSummaryPanel::GetLaunchType() const {
  return extensions::GetLaunchType(extensions::ExtensionPrefs::Get(profile_),
                                   app_);
}

void AppInfoSummaryPanel::SetLaunchType(
    extensions::LaunchType launch_type) const {
  DCHECK(CanSetLaunchType());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  extensions::SetLaunchType(service, app_->id(), launch_type);
}

bool AppInfoSummaryPanel::CanSetLaunchType() const {
  // V2 apps don't have a launch type, and neither does the Chrome app.
  return app_->id() != extension_misc::kChromeAppId && !app_->is_platform_app();
}
