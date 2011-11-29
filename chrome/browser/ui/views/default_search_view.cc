// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/default_search_view.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace {

// Returns a short name and logo resource id for the given host.
void GetShortNameAndLogoId(PrefService* prefs,
                           const TemplateURL* turl,
                           std::wstring* short_name,
                           int* logo_id) {
  DCHECK(prefs);
  DCHECK(turl);
  DCHECK(short_name);
  DCHECK(logo_id);

  GURL url = TemplateURLService::GenerateSearchURL(turl);
  scoped_ptr<TemplateURL> built_in_data(
      TemplateURLPrepopulateData::GetEngineForOrigin(prefs, url));

  // Use the built-in information to generate the short name (to ensure
  // that we don't use a name given by the search engine to itself which
  // in the worst case could be misleading).
  if (built_in_data.get()) {
    *short_name = built_in_data->short_name();
    *logo_id = built_in_data->logo_id();
  } else {
    *short_name = UTF8ToWide(url.host()).c_str();
    *logo_id = kNoSearchEngineLogo;
  }
}

views::Label* CreateProviderLabel(int message_id) {
  views::Label* choice_label =
      new views::Label(l10n_util::GetStringUTF16(message_id));
  choice_label->SetBackgroundColor(SK_ColorWHITE);
  choice_label->SetEnabledColor(SK_ColorBLACK);
  choice_label->SetFont(
      choice_label->font().DeriveFont(1, gfx::Font::NORMAL));
  return choice_label;
}

views::View* CreateProviderLogo(
    int logo_id,
    const std::wstring& short_name) {
  views::View* logo_view = NULL;

  // The width for the "logo" element when text is displayed.
  const int kTextLogoWidth = 132;

  bool use_images = false;
#if defined(GOOGLE_CHROME_BUILD)
  use_images = true;
#endif

  if (use_images && logo_id != kNoSearchEngineLogo) {
    views::ImageView* logo_image = new views::ImageView();
    SkBitmap* logo_bmp =
        ResourceBundle::GetSharedInstance().GetBitmapNamed(logo_id);
    logo_image->SetImage(logo_bmp);
    logo_image->SetTooltipText(WideToUTF16Hack(short_name));
    logo_view = logo_image;
  } else {
    // No logo -- show a text label.
    views::Label* logo_label = new views::Label(short_name);
    logo_label->SetBackgroundColor(SK_ColorWHITE);
    logo_label->SetEnabledColor(SK_ColorDKGRAY);
    logo_label->SetFont(logo_label->font().DeriveFont(3, gfx::Font::BOLD));
    logo_label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    // Tooltip text provides accessibility for low-vision users.
    logo_label->SetTooltipText(WideToUTF16Hack(short_name));
    logo_view = logo_label;
  }

  return logo_view;
}
views::View* CreateProviderChoiceButton(
    views::ButtonListener* listener,
    int message_id,
    const std::wstring& short_name) {
  return new views::NativeTextButton(listener, UTF16ToWide(
      l10n_util::GetStringFUTF16(message_id, WideToUTF16(short_name))));
}

}  // namespace

// static
void DefaultSearchView::Show(TabContents* tab_contents,
                             TemplateURL* proposed_default_turl,
                             Profile* profile) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  if (template_url_service->CanMakeDefault(proposed_default_turl) &&
      !proposed_default_turl->url()->GetHost().empty()) {
    // When the window closes, it will delete itself.
    new DefaultSearchView(tab_contents, proposed_default_turl,
                          template_url_service, profile->GetPrefs());
  } else {
    delete proposed_default_turl;
  }
}

DefaultSearchView::~DefaultSearchView() {
}

void DefaultSearchView::OnPaint(gfx::Canvas* canvas) {
  // Fill in behind the background image with the standard gray toolbar color.
  canvas->FillRect(GetThemeProvider()->GetColor(ThemeService::COLOR_TOOLBAR),
                   gfx::Rect(0, 0, width(), background_image_->height()));
  // The rest of the dialog background should be white.
  DCHECK(height() > background_image_->height());
  canvas->FillRect(SK_ColorWHITE,
                   gfx::Rect(0, background_image_->height(), width(),
                             height() - background_image_->height()));
}

void DefaultSearchView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  views::DialogClientView* client = GetDialogClientView();
  if (sender == proposed_provider_button_)
    client->AcceptWindow();
  else
    client->CancelWindow();
}

string16 DefaultSearchView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_DEFAULT_SEARCH_TITLE);
}

views::View* DefaultSearchView::GetInitiallyFocusedView() {
  return default_provider_button_;
}

views::View* DefaultSearchView::GetContentsView() {
  return this;
}

int DefaultSearchView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool DefaultSearchView::Accept() {
  // Check this again in case the default became managed while this dialog was
  // displayed.
  TemplateURL* set_as_default = proposed_turl_.get();
  if (!template_url_service_->CanMakeDefault(set_as_default))
    return true;

  template_url_service_->Add(proposed_turl_.release());
  template_url_service_->SetDefaultSearchProvider(set_as_default);
  return true;
}

views::Widget* DefaultSearchView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* DefaultSearchView::GetWidget() const {
  return View::GetWidget();
}

DefaultSearchView::DefaultSearchView(TabContents* tab_contents,
                                     TemplateURL* proposed_default_turl,
                                     TemplateURLService* template_url_service,
                                     PrefService* prefs)
    : background_image_(NULL),
      default_provider_button_(NULL),
      proposed_provider_button_(NULL),
      proposed_turl_(proposed_default_turl),
      template_url_service_(template_url_service) {
  SetupControls(prefs);

  // Show the dialog.
  new ConstrainedWindowViews(
      TabContentsWrapper::GetCurrentWrapperForContents(tab_contents), this);
}

void DefaultSearchView::SetupControls(PrefService* prefs) {
  // Column set id's.
  const int kWholeDialogViewSetId = 0;
  const int kPaddedWholeDialogViewSetId = 1;
  const int kChoicesViewSetId = 2;

  // Set up the information for the proposed default.
  std::wstring proposed_short_name;
  int proposed_logo_id = kNoSearchEngineLogo;
  GetShortNameAndLogoId(prefs,
                        proposed_turl_.get(),
                        &proposed_short_name,
                        &proposed_logo_id);
  if (proposed_logo_id != kNoSearchEngineLogo)
    proposed_turl_->set_logo_id(proposed_logo_id);


  // Set up the information for the current default.
  std::wstring default_short_name;
  int default_logo_id = kNoSearchEngineLogo;
  GetShortNameAndLogoId(prefs,
                        template_url_service_->GetDefaultSearchProvider(),
                        &default_short_name,
                        &default_logo_id);

  // Now set-up the dialog contents.
  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(0, 0, views::kPanelVertMargin, 0);
  SetLayoutManager(layout);

  // Add a column set that spans the whole dialog.
  views::ColumnSet* whole_dialog_column_set =
      layout->AddColumnSet(kWholeDialogViewSetId);
  whole_dialog_column_set->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::LEADING, 1, views::GridLayout::FIXED,
      views::Widget::GetLocalizedContentsWidth(IDS_DEFAULT_SEARCH_WIDTH_CHARS),
      0);

  // Add a column set that spans the whole dialog but obeying padding.
  views::ColumnSet* padded_whole_dialog_column_set =
      layout->AddColumnSet(kPaddedWholeDialogViewSetId);
  padded_whole_dialog_column_set->AddPaddingColumn(1, views::kPanelVertMargin);
  padded_whole_dialog_column_set->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::LEADING, 1, views::GridLayout::USE_PREF, 0, 0);
  padded_whole_dialog_column_set->AddPaddingColumn(1, views::kPanelVertMargin);

  // Add a column set for the search engine choices.
  views::ColumnSet* choices_column_set =
      layout->AddColumnSet(kChoicesViewSetId);
  choices_column_set->AddPaddingColumn(1, views::kPanelVertMargin);
  choices_column_set->AddColumn(views::GridLayout::CENTER,
      views::GridLayout::CENTER, 1, views::GridLayout::USE_PREF, 0, 0);
  choices_column_set->AddPaddingColumn(
      1, views::kRelatedControlHorizontalSpacing);
  choices_column_set->AddColumn(views::GridLayout::CENTER,
      views::GridLayout::CENTER, 1, views::GridLayout::USE_PREF, 0, 0);
  choices_column_set->LinkColumnSizes(0, 2, -1);
  choices_column_set->AddPaddingColumn(1, views::kPanelVertMargin);

  // Add the "search box" image.
  layout->StartRow(0, kWholeDialogViewSetId);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new views::ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_SEARCH_ENGINE_DIALOG_TOP));
  background_image_->EnableCanvasFlippingForRTLUI(true);
  views::ImageView::Alignment horizontal_alignment = base::i18n::IsRTL() ?
      views::ImageView::LEADING : views::ImageView::TRAILING;
  background_image_->SetHorizontalAlignment(horizontal_alignment);
  layout->AddView(background_image_);

  // Add text informing the user about the requested default change.
  layout->StartRowWithPadding(0, kPaddedWholeDialogViewSetId,
                              1, views::kLabelToControlVerticalSpacing);
  views::Label* summary_label = new views::Label(l10n_util::GetStringFUTF16(
      IDS_DEFAULT_SEARCH_SUMMARY, WideToUTF16(proposed_short_name)));
  summary_label->SetBackgroundColor(SK_ColorWHITE);
  summary_label->SetEnabledColor(SK_ColorBLACK);
  summary_label->SetFont(
      summary_label->font().DeriveFont(1, gfx::Font::NORMAL));
  summary_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(summary_label);

  // Add the labels for the tops of the choices.
  layout->StartRowWithPadding(0, kChoicesViewSetId,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(CreateProviderLabel(IDS_DEFAULT_SEARCH_LABEL_CURRENT));
  layout->AddView(CreateProviderLabel(IDS_DEFAULT_SEARCH_LABEL_PROPOSED));

  // Add the logos.
  layout->StartRowWithPadding(0, kChoicesViewSetId,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(CreateProviderLogo(default_logo_id, default_short_name));
  layout->AddView(CreateProviderLogo(proposed_logo_id, proposed_short_name));

  // Add the buttons.
  layout->StartRowWithPadding(0, kChoicesViewSetId,
                              0, views::kRelatedControlVerticalSpacing);
  default_provider_button_ = CreateProviderChoiceButton(
      this,
      IDS_DEFAULT_SEARCH_PROMPT_CURRENT,
      default_short_name);
  layout->AddView(default_provider_button_);
  proposed_provider_button_ = CreateProviderChoiceButton(
      this,
      IDS_DEFAULT_SEARCH_PROMPT_PROPOSED,
      proposed_short_name);
  layout->AddView(proposed_provider_button_);
}
