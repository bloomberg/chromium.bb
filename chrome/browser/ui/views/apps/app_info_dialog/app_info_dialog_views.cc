// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_views.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_dialog_container.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_footer_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_header_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_permissions_panel.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_summary_panel.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/arc_app_info_links_panel.h"
#endif

namespace {

// The color of the separator used inside the dialog - should match the app
// list's app_list::kDialogSeparatorColor
const SkColor kDialogSeparatorColor = SkColorSetRGB(0xD1, 0xD1, 0xD1);

}  // namespace

bool CanShowAppInfoDialog() {
#if defined(OS_MACOSX)
  return false;
#else
  return true;
#endif
}

#if BUILDFLAG(ENABLE_APP_LIST)
void ShowAppInfoInAppList(const gfx::Rect& app_info_bounds,
                          Profile* profile,
                          const extensions::Extension* app,
                          const base::Closure& close_callback) {
  views::View* app_info_view = new AppInfoDialog(profile, app);
  views::DialogDelegate* dialog =
      CreateAppListContainerForView(app_info_view, close_callback);
  views::Widget* dialog_widget = new views::Widget();
  views::Widget::InitParams params =
      views::DialogDelegate::GetDialogWidgetInitParams(dialog, nullptr, nullptr,
                                                       app_info_bounds);
  dialog_widget->Init(params);
  dialog_widget->Show();
}
#endif

void ShowAppInfoInNativeDialog(content::WebContents* web_contents,
                               Profile* profile,
                               const extensions::Extension* app,
                               const base::Closure& close_callback) {
  views::View* app_info_view = new AppInfoDialog(profile, app);
  constexpr gfx::Size kDialogSize = gfx::Size(380, 490);
  views::DialogDelegate* dialog =
      CreateDialogContainerForView(app_info_view, kDialogSize, close_callback);
  views::Widget* dialog_widget;
  if (dialog->GetModalType() == ui::MODAL_TYPE_CHILD) {
    dialog_widget =
        constrained_window::ShowWebModalDialogViews(dialog, web_contents);
  } else {
    gfx::NativeWindow window = web_contents->GetTopLevelNativeWindow();
    dialog_widget =
        constrained_window::CreateBrowserModalDialogViews(dialog, window);
    dialog_widget->Show();
  }
}

AppInfoDialog::AppInfoDialog(Profile* profile, const extensions::Extension* app)
    : profile_(profile), app_id_(app->id()) {
  views::BoxLayout* layout = SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  const int kHorizontalSeparatorHeight = 1;
  dialog_header_ = new AppInfoHeaderPanel(profile, app);
  dialog_header_->SetBorder(views::CreateSolidSidedBorder(
      0, 0, kHorizontalSeparatorHeight, 0, kDialogSeparatorColor));

  dialog_footer_ = new AppInfoFooterPanel(profile, app);
  dialog_footer_->SetBorder(views::CreateSolidSidedBorder(
      kHorizontalSeparatorHeight, 0, 0, 0, kDialogSeparatorColor));
  if (!dialog_footer_->has_children()) {
    // If there are no controls in the footer, don't add it to the dialog.
    delete dialog_footer_;
    dialog_footer_ = NULL;
  }

  // Make a vertically stacked view of all the panels we want to display in the
  // dialog.
  views::View* dialog_body_contents = new views::View();
  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  dialog_body_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical,
      provider->GetInsetsMetric(views::INSETS_DIALOG_SUBSECTION),
      provider->GetDistanceMetric(views::DISTANCE_UNRELATED_CONTROL_VERTICAL)));
  dialog_body_contents->AddChildView(new AppInfoSummaryPanel(profile, app));
  dialog_body_contents->AddChildView(new AppInfoPermissionsPanel(profile, app));

#if defined(OS_CHROMEOS)
  // When Google Play Store is enabled and the Settings app is available, show
  // the "Manage supported links" link for Chrome.
  if (app->id() == extension_misc::kChromeAppId &&
      arc::IsArcPlayStoreEnabledForProfile(profile)) {
    const ArcAppListPrefs* arc_app_list_prefs = ArcAppListPrefs::Get(profile);
    if (arc_app_list_prefs &&
        arc_app_list_prefs->IsRegistered(arc::kSettingsAppId)) {
      arc_app_info_links_ = new ArcAppInfoLinksPanel(profile, app);
      dialog_body_contents->AddChildView(arc_app_info_links_);
    }
  }
#endif

  // Clip the scrollable view so that the scrollbar appears. As long as this
  // is larger than the height of the dialog, it will be resized to the dialog's
  // actual height.
  // TODO(sashab): Add ClipHeight() as a parameter-less method to
  // views::ScrollView() to mimic this behaviour.
  const int kMaxDialogHeight = 1000;
  dialog_body_ = new views::ScrollView();
  dialog_body_->ClipHeightTo(kMaxDialogHeight, kMaxDialogHeight);
  dialog_body_->SetContents(dialog_body_contents);

  AddChildView(dialog_header_);

  AddChildView(dialog_body_);
  layout->SetFlexForView(dialog_body_, 1);

  if (dialog_footer_)
    AddChildView(dialog_footer_);

  // Close the dialog if the app is uninstalled, or if the profile is destroyed.
  StartObservingExtensionRegistry();
}

AppInfoDialog::~AppInfoDialog() {
  StopObservingExtensionRegistry();
}

void AppInfoDialog::Close() {
  GetWidget()->Close();
}

void AppInfoDialog::StartObservingExtensionRegistry() {
  DCHECK(!extension_registry_);

  extension_registry_ = extensions::ExtensionRegistry::Get(profile_);
  extension_registry_->AddObserver(this);
}

void AppInfoDialog::StopObservingExtensionRegistry() {
  if (extension_registry_)
    extension_registry_->RemoveObserver(this);
  extension_registry_ = NULL;
}

void AppInfoDialog::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  if (extension->id() != app_id_)
    return;

  Close();
}

void AppInfoDialog::OnShutdown(extensions::ExtensionRegistry* registry) {
  DCHECK_EQ(extension_registry_, registry);
  StopObservingExtensionRegistry();
  Close();
}
