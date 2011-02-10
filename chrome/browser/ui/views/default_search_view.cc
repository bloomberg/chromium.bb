// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/default_search_view.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_prepopulate_data.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/message_box_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "views/controls/button/native_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/dialog_client_view.h"
#include "views/window/window.h"

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

  GURL url = TemplateURLModel::GenerateSearchURL(turl);
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
      new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(message_id)));
  choice_label->SetColor(SK_ColorBLACK);
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
    logo_image->SetTooltipText(short_name);
    logo_view = logo_image;
  } else {
    // No logo -- show a text label.
    views::Label* logo_label = new views::Label(short_name);
    logo_label->SetColor(SK_ColorDKGRAY);
    logo_label->SetFont(logo_label->font().DeriveFont(3, gfx::Font::BOLD));
    logo_label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
    // Tooltip text provides accessibility for low-vision users.
    logo_label->SetTooltipText(short_name);
    logo_view = logo_label;
  }

  return logo_view;
}
views::NativeButton* CreateProviderChoiceButton(
    views::ButtonListener* listener,
    int message_id,
    const std::wstring& short_name) {
  return new views::NativeButton(listener, UTF16ToWide(
      l10n_util::GetStringFUTF16(message_id, WideToUTF16(short_name))));
}

}  // namespace

// static
void DefaultSearchView::Show(TabContents* tab_contents,
                             TemplateURL* default_url,
                             TemplateURLModel* template_url_model) {
  scoped_ptr<TemplateURL> template_url(default_url);
  if (!template_url_model->CanMakeDefault(default_url) ||
      default_url->url()->GetHost().empty())
    return;

  // When the window closes, it will delete itself.
  new DefaultSearchView(tab_contents, template_url.release(),
                        template_url_model);
}

DefaultSearchView::~DefaultSearchView() {
}

void DefaultSearchView::Paint(gfx::Canvas* canvas) {
  // Fill in behind the background image with the standard gray toolbar color.
  canvas->FillRectInt(SkColorSetRGB(237, 238, 237), 0, 0, width(),
                      background_image_->height());
  // The rest of the dialog background should be white.
  DCHECK(height() > background_image_->height());
  canvas->FillRectInt(SK_ColorWHITE, 0, background_image_->height(), width(),
                      height() - background_image_->height());
}

void DefaultSearchView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  views::DialogClientView* client = GetDialogClientView();
  if (sender == proposed_provider_button_)
    client->AcceptWindow();
  else
    client->CancelWindow();
}

std::wstring DefaultSearchView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_DEFAULT_SEARCH_TITLE));
}

views::View* DefaultSearchView::GetInitiallyFocusedView() {
  return default_provider_button_;
}

views::View* DefaultSearchView::GetContentsView() {
  return this;
}

int DefaultSearchView::GetDialogButtons() const {
  return ui::MessageBoxFlags::DIALOGBUTTON_NONE;
}

bool DefaultSearchView::Accept() {
  // Check this again in case the default became managed while this dialog was
  // displayed.
  TemplateURL* set_as_default = proposed_turl_.get();
  if (!template_url_model_->CanMakeDefault(set_as_default))
    return true;

  template_url_model_->Add(proposed_turl_.release());
  template_url_model_->SetDefaultSearchProvider(set_as_default);
  return true;
}

DefaultSearchView::DefaultSearchView(TabContents* tab_contents,
                                     TemplateURL* proposed_default_turl,
                                     TemplateURLModel* template_url_model)
    : background_image_(NULL),
      default_provider_button_(NULL),
      proposed_provider_button_(NULL),
      proposed_turl_(proposed_default_turl),
      template_url_model_(template_url_model) {
  PrefService* prefs = tab_contents->profile()->GetPrefs();
  SetupControls(prefs);

  // Show the dialog.
  tab_contents->CreateConstrainedDialog(this);
}

void DefaultSearchView::SetupControls(PrefService* prefs) {
  using views::ColumnSet;
  using views::GridLayout;
  using views::ImageView;
  using views::Label;

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
                        template_url_model_->GetDefaultSearchProvider(),
                        &default_short_name,
                        &default_logo_id);

  // Now set-up the dialog contents.
  GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(0, 0, kPanelVertMargin, 0);
  SetLayoutManager(layout);

  // Add a column set that spans the whole dialog.
  ColumnSet* whole_dialog_column_set =
      layout->AddColumnSet(kWholeDialogViewSetId);
  whole_dialog_column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING,
                                     1, GridLayout::FIXED,
                                     views::Window::GetLocalizedContentsWidth(
                                         IDS_DEFAULT_SEARCH_WIDTH_CHARS),
                                     0);

  // Add a column set that spans the whole dialog but obeying padding.
  ColumnSet* padded_whole_dialog_column_set =
      layout->AddColumnSet(kPaddedWholeDialogViewSetId);
  padded_whole_dialog_column_set->AddPaddingColumn(1, kPanelVertMargin);
  padded_whole_dialog_column_set->AddColumn(
      GridLayout::LEADING, GridLayout::LEADING,
      1, GridLayout::USE_PREF, 0, 0);
  padded_whole_dialog_column_set->AddPaddingColumn(1, kPanelVertMargin);

  // Add a column set for the search engine choices.
  ColumnSet* choices_column_set = layout->AddColumnSet(kChoicesViewSetId);
  choices_column_set->AddPaddingColumn(1, kPanelVertMargin);
  choices_column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER,
                                1, GridLayout::USE_PREF, 0, 0);
  choices_column_set->AddPaddingColumn(
      1, views::kRelatedControlHorizontalSpacing);
  choices_column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER,
                                1, GridLayout::USE_PREF, 0, 0);
  choices_column_set->LinkColumnSizes(0, 2, -1);
  choices_column_set->AddPaddingColumn(1, kPanelVertMargin);

  // Add the "search box" image.
  layout->StartRow(0, kWholeDialogViewSetId);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  background_image_ = new ImageView();
  background_image_->SetImage(rb.GetBitmapNamed(IDR_SEARCH_ENGINE_DIALOG_TOP));
  background_image_->EnableCanvasFlippingForRTLUI(true);
  ImageView::Alignment horizontal_alignment =
      base::i18n::IsRTL() ? ImageView::LEADING : ImageView::TRAILING;
  background_image_->SetHorizontalAlignment(horizontal_alignment);
  layout->AddView(background_image_);

  // Add text informing the user about the requested default change.
  layout->StartRowWithPadding(0, kPaddedWholeDialogViewSetId,
                              1, kLabelToControlVerticalSpacing);
  Label* summary_label = new Label(UTF16ToWide(l10n_util::GetStringFUTF16(
      IDS_DEFAULT_SEARCH_SUMMARY,
      WideToUTF16(proposed_short_name))));
  summary_label->SetColor(SK_ColorBLACK);
  summary_label->SetFont(
      summary_label->font().DeriveFont(1, gfx::Font::NORMAL));
  summary_label->SetHorizontalAlignment(Label::ALIGN_LEFT);
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
