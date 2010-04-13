// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/first_run_view.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/win_util.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/views/first_run_customize_view.h"
#include "chrome/browser/views/first_run_search_engine_view.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"
#include "views/controls/separator.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace {

// Adds a bullet glyph to a string.
std::wstring AddBullet(const std::wstring& text) {
  std::wstring btext(L" " + text);
  return btext.insert(0, 1, L'\u2022');
}

}  // namespace

FirstRunView::FirstRunView(Profile* profile, bool homepage_defined,
                           int import_items, int dont_import_items,
                           bool search_engine_experiment)
    : FirstRunViewBase(profile, homepage_defined, import_items,
                       dont_import_items, search_engine_experiment),
      welcome_label_(NULL),
      actions_label_(NULL),
      actions_import_(NULL),
      actions_shorcuts_(NULL),
      customize_link_(NULL),
      customize_selected_(false),
      accepted_(false) {
  importer_host_ = new ImporterHost();
  SetupControls();
}

FirstRunView::~FirstRunView() {
}

void FirstRunView::SetupControls() {
  using views::Label;
  using views::Link;

  if (default_browser_)
    default_browser_->SetChecked(true);

  welcome_label_ = new Label(l10n_util::GetString(IDS_FIRSTRUN_DLG_TEXT));
  welcome_label_->SetColor(SK_ColorBLACK);
  welcome_label_->SetMultiLine(true);
  welcome_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  welcome_label_->SizeToFit(0);
  AddChildView(welcome_label_);

  if (!search_engine_experiment_) {
    actions_label_ = new Label(l10n_util::GetString(IDS_FIRSTRUN_DLG_DETAIL));
  } else {
    if (importer_host_->GetAvailableProfileCount() > 0) {
      actions_label_ = new Label(l10n_util::GetStringF(
          IDS_FIRSTRUN_DLG_ACTION1_ALT,
          l10n_util::GetString(IDS_PRODUCT_NAME),
          importer_host_->GetSourceProfileNameAt(0)));
      actions_label_->SetMultiLine(true);
      actions_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
      actions_label_->SizeToFit(0);
    } else {
      NOTREACHED();
    }
  }

  actions_label_->SetHorizontalAlignment(Label::ALIGN_LEFT);
  AddChildView(actions_label_);

  if (!search_engine_experiment_) {
    // The first action label will tell what we are going to import from which
    // browser, which we obtain from the ImporterHost. We need that the first
    // browser profile be the default browser.
    std::wstring label1;
    if (importer_host_->GetAvailableProfileCount() > 0) {
      label1 = l10n_util::GetStringF(IDS_FIRSTRUN_DLG_ACTION1,
                                     importer_host_->GetSourceProfileNameAt(0));
    } else {
      NOTREACHED();
    }
    actions_import_ = new Label(AddBullet(label1));
    actions_import_->SetMultiLine(true);
    actions_import_->SetHorizontalAlignment(Label::ALIGN_LEFT);
    AddChildView(actions_import_);
    std::wstring label2 = l10n_util::GetString(IDS_FIRSTRUN_DLG_ACTION2);
    actions_shorcuts_ = new Label(AddBullet(label2));
    actions_shorcuts_->SetHorizontalAlignment(Label::ALIGN_LEFT);
    actions_shorcuts_->SetMultiLine(true);
    AddChildView(actions_shorcuts_);
  }

  customize_link_ = new Link(l10n_util::GetString(IDS_FIRSTRUN_DLG_OVERRIDE));
  customize_link_->SetController(this);
  AddChildView(customize_link_);
}

gfx::Size FirstRunView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FIRSTRUN_DIALOG_WIDTH_CHARS,
      IDS_FIRSTRUN_DIALOG_HEIGHT_LINES));
}

void FirstRunView::Layout() {
  FirstRunViewBase::Layout();

  const int kVertSpacing = 8;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  gfx::Size pref_size = welcome_label_->GetPreferredSize();
  // Wrap the label text before we overlap the product icon.
  int label_width = background_image()->width() -
      rb.GetBitmapNamed(IDR_WIZARD_ICON)->width() - kPanelHorizMargin;
  welcome_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                            label_width, pref_size.height());
  AdjustDialogWidth(welcome_label_);

  int next_v_space = background_image()->y() +
                     background_image()->height() + kPanelVertMargin;

  label_width = width() - (2 * kPanelHorizMargin);

  pref_size = actions_label_->GetPreferredSize();
  actions_label_->SetBounds(kPanelHorizMargin, next_v_space,
                            label_width, pref_size.height());
  AdjustDialogWidth(actions_label_);

  next_v_space = actions_label_->y() +
                 actions_label_->height() + kVertSpacing;

  if (!search_engine_experiment_) {
    int label_height = actions_import_->GetHeightForWidth(label_width);
    actions_import_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                               label_height);

    next_v_space = actions_import_->y() +
                   actions_import_->height() + kVertSpacing;
    AdjustDialogWidth(actions_import_);

    label_height = actions_shorcuts_->GetHeightForWidth(label_width);
    actions_shorcuts_->SetBounds(kPanelHorizMargin, next_v_space, label_width,
                                 label_height);
    AdjustDialogWidth(actions_shorcuts_);

    next_v_space = actions_shorcuts_->y() +
                   actions_shorcuts_->height() +
                   kUnrelatedControlVerticalSpacing;
  }

  pref_size = customize_link_->GetPreferredSize();
  customize_link_->SetBounds(kPanelHorizMargin, next_v_space,
                             pref_size.width(), pref_size.height());
}

void FirstRunView::OpenCustomizeDialog() {
  // The customize dialog now owns the importer host object.
  views::Window::CreateChromeWindow(
      window()->GetNativeWindow(),
      gfx::Rect(),
      new FirstRunCustomizeView(profile_,
                                importer_host_,
                                this,
                                default_browser_ && default_browser_->checked(),
                                homepage_defined_,
                                import_items_,
                                dont_import_items_,
                                search_engine_experiment_))->Show();
}

void FirstRunView::OpenSearchEngineDialog() {
  views::Window::CreateChromeWindow(
      window()->GetNativeWindow(),
      gfx::Rect(),
      new FirstRunSearchEngineView(this,
                                   profile_))->Show();
}

void FirstRunView::LinkActivated(views::Link* source, int event_flags) {
  OpenCustomizeDialog();
}

std::wstring FirstRunView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_FIRSTRUN_DLG_TITLE);
}

views::View* FirstRunView::GetContentsView() {
  return this;
}

bool FirstRunView::Accept() {
  if (!IsDialogButtonEnabled(MessageBoxFlags::DIALOGBUTTON_OK))
    return false;

  DisableButtons();
  customize_link_->SetEnabled(false);
  CreateDesktopShortcut();
  // Windows 7 has deprecated the quick launch bar.
  if (win_util::GetWinVersion() < win_util::WINVERSION_WIN7)
    CreateQuickLaunchShortcut();
  // Index 0 is the default browser.
  FirstRun::ImportSettings(profile_,
      importer_host_->GetSourceProfileInfoAt(0).browser_type,
      GetImportItems(), window()->GetNativeWindow());
  UserMetrics::RecordAction(UserMetricsAction("FirstRunDef_Accept"), profile_);
  if (default_browser_ && default_browser_->checked())
    SetDefaultBrowser();

  // Launch the search engine dialog.
  if (search_engine_experiment_) {
    OpenSearchEngineDialog();
    // Leave without shutting down; we'll observe the search engine dialog and
    // shut down after it closes.
    return false;
  }

  accepted_ = true;
  FirstRunComplete();
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  return true;
}

bool FirstRunView::Cancel() {
  UserMetrics::RecordAction(UserMetricsAction("FirstRunDef_Cancel"), profile_);
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  return true;
}

// Notification from the customize dialog that the user accepted. Since all
// the work is done there we have nothing else to do.
void FirstRunView::CustomizeAccepted() {
  if (search_engine_experiment_) {
    OpenSearchEngineDialog();
    // We'll shut down after search engine has been chosen.
    return;
  }
  accepted_ = true;
  FirstRunComplete();
  window()->Close();
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

// Notification from the customize dialog that the user cancelled.
void FirstRunView::CustomizeCanceled() {
  UserMetrics::RecordAction(UserMetricsAction("FirstRunCustom_Cancel"),
                            profile_);
}

void FirstRunView::SearchEngineChosen(const TemplateURL* default_search) {
  // default_search may be NULL if the user closed the search view without
  // making a choice, or if a choice was made through the KeywordEditor.
  if (default_search)
    profile_->GetTemplateURLModel()->SetDefaultSearchProvider(default_search);
  accepted_ = true;
  FirstRunComplete();
  window()->Close();
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

