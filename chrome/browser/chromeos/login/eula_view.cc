// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/eula_view.h"

#include <signal.h>
#include <sys/types.h>
#include <string>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/options_util.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/dom_view.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "cros/chromeos_cryptohome.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/layout_manager.h"
#include "views/standard_layout.h"
#include "views/widget/widget_gtk.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

using views::WidgetGtk;

namespace {

const int kBorderSize = 10;
const int kMargin = 20;
const int kLastButtonHorizontalMargin = 10;
const int kCheckBowWidth = 22;
const int kTextMargin = 10;

// TODO(glotov): this URL should be changed to actual Google ChromeOS EULA.
// See crbug.com/4647
const char kGoogleEulaUrl[] = "about:terms";

enum kLayoutColumnsets {
  SINGLE_CONTROL_ROW,
  SINGLE_CONTROL_WITH_SHIFT_ROW,
  SINGLE_LINK_WITH_SHIFT_ROW,
  LAST_ROW
};

// A simple LayoutManager that causes the associated view's one child to be
// sized to match the bounds of its parent except the bounds, if set.
struct FillLayoutWithBorder : public views::LayoutManager {
  // Overridden from LayoutManager:
  virtual void Layout(views::View* host) {
    DCHECK(host->GetChildViewCount());
    host->GetChildViewAt(0)->SetBounds(host->GetLocalBounds(false));
  }
  virtual gfx::Size GetPreferredSize(views::View* host) {
    return gfx::Size(host->width(), host->height());
  }
};

// System security setting dialog.
class TpmInfoView : public views::View,
                    public views::DialogDelegate {
 public:
  explicit TpmInfoView(std::wstring password) : password_(password) { }
  void Init();

 protected:
  // views::DialogDelegate overrides:
  virtual bool Accept() { return true; }
  virtual bool IsModal() const { return true; }
  virtual views::View* GetContentsView() { return this; }
  virtual int GetDialogButtons() const {
    return MessageBoxFlags::DIALOGBUTTON_OK;
  }

  // views::View overrides:
  virtual std::wstring GetWindowTitle() const {
    return l10n_util::GetString(IDS_EULA_SYSTEM_SECURITY_SETTING);
  }

  gfx::Size GetPreferredSize() {
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_TPM_INFO_DIALOG_WIDTH_CHARS,
        IDS_TPM_INFO_DIALOG_HEIGHT_LINES));
  }

 private:
  std::wstring password_;
  DISALLOW_COPY_AND_ASSIGN(TpmInfoView);
};

void TpmInfoView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  views::Label* label = new views::Label(
      l10n_util::GetString(IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION));
  label->SetMultiLine(true);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 1);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font password_font =
      rb.GetFont(ResourceBundle::MediumFont).DeriveFont(0, gfx::Font::BOLD);
  label = new views::Label(password_, password_font);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// EulaView, public:

EulaView::EulaView(chromeos::ScreenObserver* observer)
    : google_eula_label_(NULL),
      google_eula_view_(NULL),
      usage_statistics_checkbox_(NULL),
      learn_more_link_(NULL),
      oem_eula_label_(NULL),
      oem_eula_view_(NULL),
      system_security_settings_link_(NULL),
      back_button_(NULL),
      continue_button_(NULL),
      observer_(observer),
      bubble_(NULL) {
}

EulaView::~EulaView() {
  // bubble_ will be set to NULL in callback.
  if (bubble_)
    bubble_->Close();
}

// Convenience function to set layout's columnsets for this screen.
static void SetUpGridLayout(views::GridLayout* layout) {
  static const int kPadding = kBorderSize + kMargin;
  views::ColumnSet* column_set = layout->AddColumnSet(SINGLE_CONTROL_ROW);
  column_set->AddPaddingColumn(0, kPadding);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_CONTROL_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(SINGLE_LINK_WITH_SHIFT_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin + kCheckBowWidth);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kPadding);

  column_set = layout->AddColumnSet(LAST_ROW);
  column_set->AddPaddingColumn(0, kPadding + kTextMargin);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kLastButtonHorizontalMargin + kBorderSize);
}

// Convenience function. Returns URL of the OEM EULA page that should be
// displayed using current locale and manifest. Returns empty URL otherwise.
static GURL GetOemEulaPagePath() {
  const StartupCustomizationDocument *customization =
      WizardController::default_controller()->GetCustomization();
  if (customization) {
    std::string locale = g_browser_process->GetApplicationLocale();
    FilePath eula_page_path = customization->GetEULAPagePath(locale);
    if (eula_page_path.empty()) {
      LOG(INFO) << "No eula found for locale: " << locale;
      locale = customization->initial_locale();
      eula_page_path = customization->GetEULAPagePath(locale);
    }
    if (!eula_page_path.empty()) {
      const std::string page_path = std::string(chrome::kFileScheme) +
          chrome::kStandardSchemeSeparator + eula_page_path.value();
      return GURL(page_path);
    } else {
      LOG(INFO) << "No eula found for locale: " << locale;
    }
  } else {
    LOG(ERROR) << "No manifest found.";
  }
  return GURL();
}

void EulaView::Init() {
  // Use rounded rect background.
  views::Painter* painter = CreateWizardPainter(
      &BorderDefinition::kScreenBorder);
  set_background(
      views::Background::CreateBackgroundPainter(true, painter));

  // Layout created controls.
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  SetUpGridLayout(layout);

  static const int kPadding = kBorderSize + kMargin;
  layout->AddPaddingRow(0, kPadding);
  layout->StartRow(0, SINGLE_CONTROL_ROW);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font label_font =
      rb.GetFont(ResourceBundle::MediumFont).DeriveFont(0, gfx::Font::NORMAL);
  google_eula_label_ = new views::Label(std::wstring(), label_font);
  layout->AddView(google_eula_label_, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::FILL);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(1, SINGLE_CONTROL_ROW);
  views::View* box_view = new views::View();
  box_view->set_border(views::Border::CreateSolidBorder(1, SK_ColorBLACK));
  box_view->SetLayoutManager(new FillLayoutWithBorder());
  layout->AddView(box_view);

  google_eula_view_ = new DOMView();
  box_view->AddChildView(google_eula_view_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, SINGLE_CONTROL_WITH_SHIFT_ROW);
  usage_statistics_checkbox_ = new views::Checkbox();
  usage_statistics_checkbox_->SetMultiLine(true);

  // TODO(zelidrag): http://crosbug/6367 - Change default settings for checked
  // and enabled state of usage_statistics_checkbox_ before the product is
  // released.
  usage_statistics_checkbox_->SetChecked(true);
  layout->AddView(usage_statistics_checkbox_);
  usage_statistics_checkbox_->SetEnabled(false);

  layout->StartRow(0, SINGLE_LINK_WITH_SHIFT_ROW);
  learn_more_link_ = new views::Link();
  learn_more_link_->SetController(this);
  layout->AddView(learn_more_link_);

  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, SINGLE_CONTROL_ROW);
  oem_eula_label_ = new views::Label(std::wstring(), label_font);
  layout->AddView(oem_eula_label_, 1, 1,
                  views::GridLayout::LEADING, views::GridLayout::FILL);

  oem_eula_page_ = GetOemEulaPagePath();
  if (!oem_eula_page_.is_empty()) {
    layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
    layout->StartRow(1, SINGLE_CONTROL_ROW);
    box_view = new views::View();
    box_view->SetLayoutManager(new FillLayoutWithBorder());
    box_view->set_border(views::Border::CreateSolidBorder(1, SK_ColorBLACK));
    layout->AddView(box_view);

    oem_eula_view_ = new DOMView();
    box_view->AddChildView(oem_eula_view_);
  }

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, LAST_ROW);
  system_security_settings_link_ = new views::Link();
  system_security_settings_link_->SetController(this);
  if (!chromeos::CrosLibrary::Get()->EnsureLoaded() ||
      !chromeos::CryptohomeTpmIsEnabled()) {
    system_security_settings_link_->SetEnabled(false);
    // TODO(glotov): add tooltip with description.
  }
  layout->AddView(system_security_settings_link_);

  back_button_ = new views::NativeButton(this, std::wstring());
  layout->AddView(back_button_);

  continue_button_ = new views::NativeButton(this, std::wstring());
  layout->AddView(continue_button_);
  layout->AddPaddingRow(0, kPadding);

  UpdateLocalizedStrings();
}

void EulaView::UpdateLocalizedStrings() {
  // Load Google EULA and its title.
  LoadEulaView(google_eula_view_, google_eula_label_, GURL(kGoogleEulaUrl));

  // Load OEM EULA and its title.
  if (!oem_eula_page_.is_empty())
    LoadEulaView(oem_eula_view_, oem_eula_label_, oem_eula_page_);

  // Set tooltip for usage statistics checkbox if the metric is unmanaged.
  if (!usage_statistics_checkbox_->IsEnabled()) {
    usage_statistics_checkbox_->SetTooltipText(
        l10n_util::GetString(IDS_OPTION_DISABLED_BY_POLICY));
  }

  // Load other labels from resources.
  usage_statistics_checkbox_->SetLabel(
      l10n_util::GetString(IDS_EULA_CHECKBOX_ENABLE_LOGGING));
  learn_more_link_->SetText(
      l10n_util::GetString(IDS_LEARN_MORE));
  system_security_settings_link_->SetText(
      l10n_util::GetString(IDS_EULA_SYSTEM_SECURITY_SETTING));
  continue_button_->SetLabel(
      l10n_util::GetString(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON));
  back_button_->SetLabel(
      l10n_util::GetString(IDS_ACCNAME_BACK));
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, protected, views::View implementation:

void EulaView::OnLocaleChanged() {
  UpdateLocalizedStrings();
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// views::ButtonListener implementation:

void EulaView::ButtonPressed(views::Button* sender, const views::Event& event) {
  if (sender == continue_button_) {
    if (usage_statistics_checkbox_) {
      bool enable_reporting = usage_statistics_checkbox_->checked();
      enable_reporting =
          OptionsUtil::ResolveMetricsReportingEnabled(enable_reporting);
#if defined(USE_LINUX_BREAKPAD)
      if (enable_reporting)
        InitCrashReporter();
#endif
    }
    observer_->OnExit(ScreenObserver::EULA_ACCEPTED);
  } else if (sender == back_button_) {
    observer_->OnExit(ScreenObserver::EULA_BACK);
  }
}

////////////////////////////////////////////////////////////////////////////////
// views::LinkController implementation:

void EulaView::LinkActivated(views::Link* source, int event_flags) {
  if (source == learn_more_link_) {
    if (!help_app_.get())
      help_app_.reset(new HelpAppLauncher(GetNativeWindow()));
    help_app_->ShowHelpTopic(HelpAppLauncher::HELP_STATS_USAGE);
  } else if (source == system_security_settings_link_) {
    // Pull the password from TPM.
    std::string password;
    if (!chromeos::CrosLibrary::Get()->EnsureLoaded()) {
      LOG(ERROR) << "Cros library not loaded. "
                 << "We must have disabled the link that led here.";
      return;
    } else if (chromeos::CryptohomeTpmIsReady() &&
               chromeos::CryptohomeTpmGetPassword(&password)) {
      TpmInfoView* view = new TpmInfoView(ASCIIToWide(password));
      view->Init();
      views::Window* window = views::Window::CreateChromeWindow(
          GetNativeWindow(), gfx::Rect(), view);
      window->SetIsAlwaysOnTop(true);
      window->Show();
    } else {
      if (!bubble_)
        bubble_ = MessageBubble::Show(
            system_security_settings_link_->GetWidget(),
            system_security_settings_link_->GetScreenBounds(),
            BubbleBorder::LEFT_TOP,
            ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
            l10n_util::GetString(IDS_EULA_TPM_BUSY),
            std::wstring(), this);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate implementation:

// Convenience function. Queries |eula_view| for HTML title and, if it
// is ready, assigns it to |eula_label| and returns true so the caller
// view calls Layout().
static bool PublishTitleIfReady(const TabContents* contents,
                                DOMView* eula_view,
                                views::Label* eula_label) {
  if (contents != eula_view->tab_contents())
    return false;
  eula_label->SetText(UTF16ToWide(eula_view->tab_contents()->GetTitle()));
  return true;
}

void EulaView::NavigationStateChanged(const TabContents* contents,
                                      unsigned changed_flags) {
  if (changed_flags & TabContents::INVALIDATE_TITLE) {
    if (PublishTitleIfReady(contents, google_eula_view_, google_eula_label_) ||
        PublishTitleIfReady(contents, oem_eula_view_, oem_eula_label_)) {
      Layout();
    }
  }
}

void EulaView::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  views::Widget* widget = GetWidget();
  if (widget && event.os_event && !event.skip_in_browser)
    static_cast<views::WidgetGtk*>(widget)->HandleKeyboardEvent(event.os_event);
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, private:

gfx::NativeWindow EulaView::GetNativeWindow() const {
  return GTK_WINDOW(static_cast<WidgetGtk*>(GetWidget())->GetNativeView());
}

void EulaView::LoadEulaView(DOMView* eula_view,
                            views::Label* eula_label,
                            const GURL& eula_url) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  eula_view->Init(profile,
                  SiteInstance::CreateSiteInstanceForURL(profile, eula_url));
  eula_view->LoadURL(eula_url);
  eula_view->tab_contents()->set_delegate(this);
}

////////////////////////////////////////////////////////////////////////////////
// EulaView, private, views::View implementation:

bool EulaView::OnKeyPressed(const views::KeyEvent&) {
  // Close message bubble if shown. bubble_ will be set to NULL in callback.
  if (bubble_) {
    bubble_->Close();
    return true;
  }
  return false;
}

}  // namespace chromeos
