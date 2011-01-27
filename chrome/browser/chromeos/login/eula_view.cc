// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/eula_view.h"

#include <signal.h>
#include <sys/types.h>
#include <string>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/network_screen_delegate.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/metrics_cros_settings_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/views/dom_view.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/native_web_keyboard_event.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/layout/layout_manager.h"
#include "views/widget/widget_gtk.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

using views::WidgetGtk;

namespace {

const int kBorderSize = 10;
const int kCheckboxWidth = 20;
const int kLastButtonHorizontalMargin = 10;
const int kMargin = 20;
const int kTextMargin = 10;
const int kTpmCheckIntervalMs = 500;

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
  explicit TpmInfoView(std::string* password)
      : ALLOW_THIS_IN_INITIALIZER_LIST(runnable_method_factory_(this)),
        password_(password) {
    DCHECK(password_);
  }

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
    return UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING));
  }

  gfx::Size GetPreferredSize() {
    return gfx::Size(views::Window::GetLocalizedContentsSize(
        IDS_TPM_INFO_DIALOG_WIDTH_CHARS,
        IDS_TPM_INFO_DIALOG_HEIGHT_LINES));
  }

 private:
  void PullPassword();

  ScopedRunnableMethodFactory<TpmInfoView> runnable_method_factory_;

  // Holds pointer to the password storage.
  std::string* password_;

  views::Label* busy_label_;
  views::Label* password_label_;
  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(TpmInfoView);
};

void TpmInfoView::Init() {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 0);
  views::Label* label = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION)));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, 0);
  label = new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_EULA_SYSTEM_SECURITY_SETTING_DESCRIPTION_KEY)));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, 1);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font password_font =
      rb.GetFont(ResourceBundle::MediumFont).DeriveFont(0, gfx::Font::BOLD);
  // Password will be set later.
  password_label_ = new views::Label(L"", password_font);
  password_label_->SetVisible(false);
  layout->AddView(password_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  column_set = layout->AddColumnSet(2);
  column_set->AddPaddingColumn(1, 0);
  // Resize of the throbber and label is not allowed, since we want they to be
  // placed in the center.
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(1, 0);
  // Border padding columns should have the same width. It guaranties that
  // throbber and label will be placed in the center.
  column_set->LinkColumnSizes(0, 4, -1);

  layout->StartRow(0, 2);
  throbber_ = chromeos::CreateDefaultThrobber();
  throbber_->Start();
  layout->AddView(throbber_);
  busy_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EULA_TPM_BUSY)));
  layout->AddView(busy_label_);
  layout->AddPaddingRow(0, kRelatedControlHorizontalSpacing);

  PullPassword();
}

void TpmInfoView::PullPassword() {
  // Since this method is also called directly.
  runnable_method_factory_.RevokeAll();

  chromeos::CryptohomeLibrary* cryptohome =
      chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();

  bool password_acquired = false;
  if (password_->empty() && cryptohome->TpmIsReady()) {
    password_acquired = cryptohome->TpmGetPassword(password_);
    if (!password_acquired) {
      password_->clear();
    } else if (password_->empty()) {
      // For a fresh OOBE flow TPM is uninitialized,
      // ownership process is started at the EULA screen,
      // password is cleared after EULA is accepted.
      LOG(ERROR) << "TPM returned an empty password.";
    }
  }
  if (password_->empty() && !password_acquired) {
    // Password hasn't been acquired, reschedule pulling.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        runnable_method_factory_.NewRunnableMethod(&TpmInfoView::PullPassword),
        kTpmCheckIntervalMs);
  } else {
    password_label_->SetText(ASCIIToWide(*password_));
    password_label_->SetVisible(true);
    busy_label_->SetVisible(false);
    throbber_->Stop();
    throbber_->SetVisible(false);
  }
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
  column_set->AddPaddingColumn(0, kPadding + kTextMargin + kCheckboxWidth);
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
      VLOG(1) << "No eula found for locale: " << locale;
      locale = customization->initial_locale();
      eula_page_path = customization->GetEULAPagePath(locale);
    }
    if (!eula_page_path.empty()) {
      const std::string page_path = std::string(chrome::kFileScheme) +
          chrome::kStandardSchemeSeparator + eula_page_path.value();
      return GURL(page_path);
    } else {
      VLOG(1) << "No eula found for locale: " << locale;
    }
  } else {
    LOG(ERROR) << "No manifest found.";
  }
  return GURL();
}

void EulaView::Init() {
  // First, command to own the TPM.
  if (chromeos::CrosLibrary::Get()->EnsureLoaded()) {
    chromeos::CrosLibrary::Get()->
        GetCryptohomeLibrary()->TpmCanAttemptOwnership();
  } else {
    LOG(ERROR) << "Cros library not loaded. "
               << "We must have disabled the link that led here.";
  }

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
  usage_statistics_checkbox_->SetChecked(true);
  layout->AddView(usage_statistics_checkbox_);

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
      !chromeos::CrosLibrary::Get()->GetCryptohomeLibrary()->
          TpmIsEnabled()) {
    system_security_settings_link_->SetEnabled(false);
  }

  layout->AddView(system_security_settings_link_);

  back_button_ = new login::WideButton(this, std::wstring());
  layout->AddView(back_button_);

  continue_button_ = new login::WideButton(this, std::wstring());
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
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTION_DISABLED_BY_POLICY)));
  }

  // Set tooltip for system security settings link if TPM is disabled.
  if (!system_security_settings_link_->IsEnabled()) {
    system_security_settings_link_->SetTooltipText(
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_EULA_TPM_DISABLED)));
  }

  // Load other labels from resources.
  usage_statistics_checkbox_->SetLabel(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EULA_CHECKBOX_ENABLE_LOGGING)));
  learn_more_link_->SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
  system_security_settings_link_->SetText(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EULA_SYSTEM_SECURITY_SETTING)));
  continue_button_->SetLabel(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_EULA_ACCEPT_AND_CONTINUE_BUTTON)));
  back_button_->SetLabel(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EULA_BACK_BUTTON)));
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
      MetricsCrosSettingsProvider::SetMetricsStatus(
          usage_statistics_checkbox_->checked());
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
    TpmInfoView* view = new TpmInfoView(&tpm_password_);
    view->Init();
    views::Window* window = browser::CreateViewsWindow(
        GetNativeWindow(), gfx::Rect(), view);
    window->SetIsAlwaysOnTop(true);
    window->Show();
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
