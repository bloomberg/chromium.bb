// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/app_dialog/app_uninstall_dialog_view.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/google/core/common/google_util.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_url_handlers.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#endif

namespace {

AppUninstallDialogView* g_app_uninstall_dialog_view = nullptr;

}  // namespace

// static
void apps::UninstallDialog::UiBase::Create(
    Profile* profile,
    apps::mojom::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    gfx::NativeWindow parent_window,
    apps::UninstallDialog* uninstall_dialog) {
  constrained_window::CreateBrowserModalDialogViews(
      (new AppUninstallDialogView(profile, app_type, app_id, app_name, image,
                                  uninstall_dialog)),
      parent_window)
      ->Show();
}

AppUninstallDialogView::AppUninstallDialogView(
    Profile* profile,
    apps::mojom::AppType app_type,
    const std::string& app_id,
    const std::string& app_name,
    gfx::ImageSkia image,
    apps::UninstallDialog* uninstall_dialog)
    : apps::UninstallDialog::UiBase(uninstall_dialog),
      AppDialogView(app_name, image),
      profile_(profile),
      app_type_(app_type) {
  SetCloseCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                  base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&AppUninstallDialogView::OnDialogCancelled,
                                   base::Unretained(this)));
  SetAcceptCallback(base::BindOnce(&AppUninstallDialogView::OnDialogAccepted,
                                   base::Unretained(this)));

  InitializeView(profile, app_type, app_id);

  chrome::RecordDialogCreation(chrome::DialogIdentifier::APP_UNINSTALL);

  g_app_uninstall_dialog_view = this;
}

AppUninstallDialogView::~AppUninstallDialogView() {
  g_app_uninstall_dialog_view = nullptr;
}

// static
AppUninstallDialogView* AppUninstallDialogView::GetActiveViewForTesting() {
  return g_app_uninstall_dialog_view;
}

ui::ModalType AppUninstallDialogView::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 AppUninstallDialogView::GetWindowTitle() const {
  switch (app_type_) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kMacNative:
    case apps::mojom::AppType::kLacros:
      NOTREACHED();
      return base::string16();
    case apps::mojom::AppType::kArc:
#if defined(OS_CHROMEOS)
      if (shortcut_)
        return l10n_util::GetStringUTF16(IDS_EXTENSION_UNINSTALL_PROMPT_TITLE);

      // Otherwise fallback to Uninstall + app name.
      FALLTHROUGH;
#else
      NOTREACHED();
      return base::string16();
#endif
    case apps::mojom::AppType::kCrostini:
    case apps::mojom::AppType::kPluginVm:
#if defined(OS_CHROMEOS)
      FALLTHROUGH;
#else
      NOTREACHED();
      return base::string16();
#endif
    case apps::mojom::AppType::kExtension:
    case apps::mojom::AppType::kWeb:
      return l10n_util::GetStringFUTF16(IDS_PROMPT_APP_UNINSTALL_TITLE,
                                        base::UTF8ToUTF16(app_name()));
  }
}

void AppUninstallDialogView::InitializeView(Profile* profile,
                                            apps::mojom::AppType app_type,
                                            const std::string& app_id) {
  SetButtonLabel(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_APP_BUTTON));

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL)));

  switch (app_type_) {
    case apps::mojom::AppType::kUnknown:
    case apps::mojom::AppType::kBuiltIn:
    case apps::mojom::AppType::kMacNative:
    case apps::mojom::AppType::kLacros:
      NOTREACHED();
      break;
    case apps::mojom::AppType::kArc:
#if defined(OS_CHROMEOS)
      InitializeViewForArcApp(profile, app_id);
#else
      NOTREACHED();
#endif
      break;
    case apps::mojom::AppType::kPluginVm:
#if defined(OS_CHROMEOS)
      InitializeViewWithMessage(l10n_util::GetStringFUTF16(
          IDS_PLUGIN_VM_UNINSTALL_PROMPT_BODY, base::UTF8ToUTF16(app_name())));
#else
      NOTREACHED();
#endif
      break;
    case apps::mojom::AppType::kCrostini:
#if defined(OS_CHROMEOS)
      InitializeViewWithMessage(l10n_util::GetStringUTF16(
          IDS_CROSTINI_APPLICATION_UNINSTALL_CONFIRM_BODY));
#else
      NOTREACHED();
#endif
      break;

    case apps::mojom::AppType::kWeb:
      if (base::FeatureList::IsEnabled(
              features::kDesktopPWAsWithoutExtensions)) {
        InitializeViewForWebApp(profile, app_id);
        break;
      }
      // Otherwise fallback to Extension-based Bookmark Apps.
      FALLTHROUGH;
    case apps::mojom::AppType::kExtension:
      InitializeViewForExtension(profile, app_id);
      break;
  }
}

void AppUninstallDialogView::InitializeCheckbox(const GURL& app_launch_url) {
  std::unique_ptr<views::StyledLabel> checkbox_label;
  std::vector<base::string16> replacements;
  size_t offset;
  base::string16 learn_more_text =
      l10n_util::GetStringUTF16(IDS_APP_UNINSTALL_PROMPT_LEARN_MORE);

  replacements.push_back(url_formatter::FormatUrlForSecurityDisplay(
      app_launch_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));

  if (google_util::IsGoogleHostname(app_launch_url.host_piece(),
                                    google_util::ALLOW_SUBDOMAIN)) {
    replacements.push_back(learn_more_text);

    std::vector<size_t> offsets;
    checkbox_label = std::make_unique<views::StyledLabel>(
        l10n_util::GetStringFUTF16(
            IDS_APP_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX_FOR_GOOGLE,
            replacements, &offsets),
        this);
    DCHECK_EQ(replacements.size(), offsets.size());
    offset = offsets[offsets.size() - 1];
  } else {
    auto domain = net::registry_controlled_domains::GetDomainAndRegistry(
        app_launch_url,
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    DCHECK(!domain.empty());
    domain[0] = base::ToUpperASCII(domain[0]);

    replacements.push_back(base::ASCIIToUTF16(domain));
    replacements.push_back(learn_more_text);

    std::vector<size_t> offsets;
    checkbox_label = std::make_unique<views::StyledLabel>(
        l10n_util::GetStringFUTF16(
            IDS_APP_UNINSTALL_PROMPT_REMOVE_DATA_CHECKBOX_FOR_NON_GOOGLE,
            replacements, &offsets),
        this);
    DCHECK_EQ(replacements.size(), offsets.size());
    offset = offsets[offsets.size() - 1];
  }

  checkbox_label->AddStyleRange(
      gfx::Range(offset, offset + learn_more_text.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink());
  views::StyledLabel::RangeStyleInfo checkbox_style;
  checkbox_style.text_style = views::style::STYLE_PRIMARY;
  gfx::Range before_link_range(0, offset);
  checkbox_label->AddStyleRange(before_link_range, checkbox_style);

  // Shift the text down to align with the checkbox.
  checkbox_label->SetBorder(views::CreateEmptyBorder(3, 0, 0, 0));

  auto clear_site_data_checkbox =
      std::make_unique<views::Checkbox>(base::string16());
  clear_site_data_checkbox->SetAssociatedLabel(checkbox_label.get());

  // Create a view to hold the checkbox and the text.
  auto checkbox_view = std::make_unique<views::View>();
  views::GridLayout* checkbox_layout =
      checkbox_view->SetLayoutManager(std::make_unique<views::GridLayout>());

  const int kReportColumnSetId = 0;
  views::ColumnSet* cs = checkbox_layout->AddColumnSet(kReportColumnSetId);
  cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                views::GridLayout::kFixedSize,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  cs->AddPaddingColumn(views::GridLayout::kFixedSize,
                       ChromeLayoutProvider::Get()->GetDistanceMetric(
                           views::DISTANCE_RELATED_LABEL_HORIZONTAL));
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1.0,
                views::GridLayout::ColumnSize::kUsePreferred, 0, 0);

  checkbox_layout->StartRow(views::GridLayout::kFixedSize, kReportColumnSetId);
  clear_site_data_checkbox_ =
      checkbox_layout->AddView(std::move(clear_site_data_checkbox));
  checkbox_layout->AddView(std::move(checkbox_label));
  AddChildView(std::move(checkbox_view));
}

void AppUninstallDialogView::InitializeViewForExtension(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  DCHECK(extension);

  if (extensions::ManifestURL::UpdatesFromGallery(extension)) {
    auto report_abuse_checkbox = std::make_unique<views::Checkbox>(
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_REPORT_ABUSE));
    report_abuse_checkbox->SetMultiLine(true);
    report_abuse_checkbox_ = AddChildView(std::move(report_abuse_checkbox));
  } else if (extension->from_bookmark()) {
    InitializeCheckbox(extensions::AppLaunchInfo::GetFullLaunchURL(extension));
  }
}

void AppUninstallDialogView::InitializeViewForWebApp(
    Profile* profile,
    const std::string& app_id) {
  auto* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);

  GURL app_launch_url = provider->registrar().GetAppLaunchURL(app_id);
  DCHECK(app_launch_url.is_valid());

  InitializeCheckbox(app_launch_url);
}

#if defined(OS_CHROMEOS)
void AppUninstallDialogView::InitializeViewForArcApp(
    Profile* profile,
    const std::string& app_id) {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      arc_prefs->GetApp(app_id);
  DCHECK(app_info);

  shortcut_ = app_info->shortcut;

  if (shortcut_) {
    SetButtonLabel(
        ui::DIALOG_BUTTON_OK,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON));
  } else {
    InitializeViewWithMessage(l10n_util::GetStringUTF16(
        IDS_ARC_APP_UNINSTALL_PROMPT_DATA_REMOVAL_WARNING));
  }
}

void AppUninstallDialogView::InitializeViewWithMessage(
    const base::string16& message) {
  auto* label = AddChildView(std::make_unique<views::Label>(message));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetAllowCharacterBreak(true);
}
#endif

void AppUninstallDialogView::StyledLabelLinkClicked(views::StyledLabel* label,
                                                    const gfx::Range& range,
                                                    int event_flags) {
  NavigateParams params(
      profile_, GURL("https://support.google.com/chromebook/?p=uninstallpwa"),
      ui::PAGE_TRANSITION_LINK);
  Navigate(&params);
}

void AppUninstallDialogView::OnDialogCancelled() {
  uninstall_dialog()->OnDialogClosed(false /* uninstall */,
                                     false /* clear_site_data */,
                                     false /* report_abuse */);
}

void AppUninstallDialogView::OnDialogAccepted() {
  const bool clear_site_data =
      clear_site_data_checkbox_ && clear_site_data_checkbox_->GetChecked();
  const bool report_abuse_checkbox =
      report_abuse_checkbox_ && report_abuse_checkbox_->GetChecked();
  uninstall_dialog()->OnDialogClosed(true /* uninstall */, clear_site_data,
                                     report_abuse_checkbox);
}
