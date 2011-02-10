// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/fonts_page_view.h"

#include <windows.h>
#include <shlobj.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <vector>

#include "base/file_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/default_encoding_combo_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/locale_settings.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/font.h"
#include "ui/gfx/native_theme_win.h"
#include "views/controls/button/native_button.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"

////////////////////////////////////////////////////////////////////////////////
// FontDisplayView

class FontDisplayView : public views::View {
 public:
  FontDisplayView();
  virtual ~FontDisplayView();

  // This method takes in font size in pixel units, instead of the normal point
  // unit because users expect the font size number to represent pixels and not
  // points.
  void SetFontType(const std::wstring& font_name,
                   int font_size);

  std::wstring font_name() { return font_name_; }
  int font_size() { return font_size_; }

  // views::View overrides:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 private:
  views::Label* font_text_label_;
  std::wstring font_name_;
  int font_size_;
  std::wstring font_text_label_string_;

  static const int kFontDisplayMaxWidthChars = 50;
  static const int kFontDisplayMaxHeightChars = 1;
  static const int kFontDisplayLabelPadding = 5;

  DISALLOW_COPY_AND_ASSIGN(FontDisplayView);
};

FontDisplayView::FontDisplayView()
    : font_text_label_(new views::Label) {
  AddChildView(font_text_label_);
}

FontDisplayView::~FontDisplayView() {
}

void FontDisplayView::SetFontType(const std::wstring& font_name,
                                  int font_size) {
  if (font_name.empty())
    return;

  font_name_ = font_name;
  font_size_ = font_size;
  std::wstring displayed_text = font_name_;

  // Append the font type and size.
  displayed_text += L", ";
  displayed_text += UTF8ToWide(::StringPrintf("%d", font_size_));
  HDC hdc = GetDC(NULL);
  int font_size_point = MulDiv(font_size, 72, GetDeviceCaps(hdc, LOGPIXELSY));
  gfx::Font font = gfx::Font(font_name, font_size_point);
  font_text_label_->SetFont(font);
  font_text_label_->SetText(displayed_text);
}

void FontDisplayView::Paint(gfx::Canvas* canvas) {
  HDC dc = canvas->BeginPlatformPaint();
  RECT rect = { 0, 0, width(), height() };
  gfx::NativeTheme::instance()->PaintTextField(
      dc, EP_BACKGROUND, EBS_NORMAL, 0, &rect, ::GetSysColor(COLOR_3DFACE),
      true, true);
  canvas->EndPlatformPaint();
}

void FontDisplayView::Layout() {
  font_text_label_->SetBounds(0, 0, width(), height());
}

gfx::Size FontDisplayView::GetPreferredSize() {
  gfx::Size size = font_text_label_->GetPreferredSize();
  size.set_width(size.width() + 2 * kFontDisplayLabelPadding);
  size.set_height(size.height() + 2 * kFontDisplayLabelPadding);
  return size;
}

void EmbellishTitle(views::Label* title_label) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label->SetFont(title_font);
  SkColor title_color =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
          gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
          COLOR_WINDOWTEXT);
  title_label->SetColor(title_color);
}

FontsPageView::FontsPageView(Profile* profile)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          select_font_dialog_(SelectFontDialog::Create(this))),
      fonts_group_title_(NULL),
      encoding_group_title_(NULL),
      fixed_width_font_change_page_button_(NULL),
      serif_font_change_page_button_(NULL),
      sans_serif_font_change_page_button_(NULL),
      fixed_width_font_label_(NULL),
      serif_font_label_(NULL),
      sans_serif_font_label_(NULL),
      default_encoding_combobox_(NULL),
      serif_button_pressed_(false),
      sans_serif_button_pressed_(false),
      fixed_width_button_pressed_(false),
      encoding_dropdown_clicked_(false),
      font_type_being_changed_(NONE),
      OptionsPageView(profile),
      font_changed_(false),
      default_encoding_changed_(false),
      serif_font_size_pixel_(0),
      sans_serif_font_size_pixel_(0),
      fixed_width_font_size_pixel_(0) {
  serif_name_.Init(prefs::kWebKitSerifFontFamily, profile->GetPrefs(), NULL);
  serif_size_.Init(prefs::kWebKitDefaultFontSize, profile->GetPrefs(), NULL);

  sans_serif_name_.Init(prefs::kWebKitSansSerifFontFamily, profile->GetPrefs(),
                        NULL);
  sans_serif_size_.Init(prefs::kWebKitDefaultFontSize, profile->GetPrefs(),
                        NULL);

  fixed_width_name_.Init(prefs::kWebKitFixedFontFamily, profile->GetPrefs(),
                         NULL);
  fixed_width_size_.Init(prefs::kWebKitDefaultFixedFontSize,
                         profile->GetPrefs(), NULL);

  default_encoding_.Init(prefs::kDefaultCharset, profile->GetPrefs(), NULL);
}

FontsPageView::~FontsPageView() {
}

void FontsPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  HWND owning_hwnd = GetAncestor(GetWidget()->GetNativeView(), GA_ROOT);
  std::wstring font_name;
  int font_size = 0;
  if (sender == serif_font_change_page_button_) {
    font_type_being_changed_ = SERIF;
    font_name = serif_font_display_view_->font_name();
    font_size = serif_font_size_pixel_;
  } else if (sender == sans_serif_font_change_page_button_) {
    font_type_being_changed_ = SANS_SERIF;
    font_name = sans_serif_font_display_view_->font_name();
    font_size = sans_serif_font_size_pixel_;
  } else if (sender == fixed_width_font_change_page_button_) {
    font_type_being_changed_ = FIXED_WIDTH;
    font_name = fixed_width_font_display_view_->font_name();
    font_size = fixed_width_font_size_pixel_;
  } else {
    NOTREACHED();
    return;
  }

  select_font_dialog_->SelectFont(owning_hwnd, NULL, font_name, font_size);
}

void FontsPageView::ItemChanged(views::Combobox* combo_box,
                                int prev_index, int new_index) {
  if (combo_box == default_encoding_combobox_) {
    if (prev_index != new_index) {  // Default-Encoding has been changed.
      encoding_dropdown_clicked_ = true;
      default_encoding_selected_ = default_encoding_combobox_model_->
          GetEncodingCharsetByIndex(new_index);
      default_encoding_changed_ = true;
    }
  }
}

void FontsPageView::FontSelected(const gfx::Font& font, void* params) {
  if (font.GetFontName().empty())
    return;
  int font_size = font.GetFontSize();
  // Currently we do not have separate font sizes for Serif and Sans Serif.
  // Therefore, when Serif font size is changed, Sans-Serif font size changes,
  // and vice versa.
  if (font_type_being_changed_ == SERIF) {
    sans_serif_font_size_pixel_ = serif_font_size_pixel_ = font_size;
    serif_font_display_view_->SetFontType(
        font.GetFontName(),
        serif_font_size_pixel_);
    sans_serif_font_display_view_->SetFontType(
        sans_serif_font_display_view_->font_name(),
        sans_serif_font_size_pixel_);
  } else if (font_type_being_changed_ == SANS_SERIF) {
    sans_serif_font_size_pixel_ = serif_font_size_pixel_ = font_size;
    sans_serif_font_display_view_->SetFontType(
        font.GetFontName(),
        sans_serif_font_size_pixel_);
    serif_font_display_view_->SetFontType(
        serif_font_display_view_->font_name(),
        sans_serif_font_size_pixel_);
  } else if (font_type_being_changed_ == FIXED_WIDTH) {
    fixed_width_font_size_pixel_ = font_size;
    fixed_width_font_display_view_->SetFontType(font.GetFontName(), font_size);
  }
  font_changed_ = true;
}

void FontsPageView::SaveChanges() {
  // Set Fonts.
  if (font_changed_) {
    serif_name_.SetValue(WideToUTF8(serif_font_display_view_->font_name()));
    serif_size_.SetValue(serif_font_size_pixel_);
    sans_serif_name_.SetValue(
        WideToUTF8(sans_serif_font_display_view_->font_name()));
    sans_serif_size_.SetValue(sans_serif_font_size_pixel_);
    fixed_width_name_.SetValue(WideToUTF8(
        fixed_width_font_display_view_->font_name()));
    fixed_width_size_.SetValue(fixed_width_font_size_pixel_);
  }
  // Set Encoding.
  if (default_encoding_changed_)
    default_encoding_.SetValue(default_encoding_selected_);
}

void FontsPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);

  // Fonts group.
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);
  fonts_group_title_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_FONT_TITLE)));
  EmbellishTitle(fonts_group_title_);
  fonts_group_title_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(fonts_group_title_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  InitFontLayout();
  layout->AddView(fonts_contents_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  // Encoding group.
  encoding_group_title_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SUB_DIALOG_ENCODING_TITLE)));
  EmbellishTitle(encoding_group_title_);
  encoding_group_title_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(encoding_group_title_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  InitEncodingLayout();
  layout->AddView(encoding_contents_);
}

void FontsPageView::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kWebKitFixedFontFamily) {
    fixed_width_font_size_pixel_ = fixed_width_size_.GetValue();
    fixed_width_font_display_view_->SetFontType(
        UTF8ToWide(fixed_width_name_.GetValue()),
        fixed_width_font_size_pixel_);
  }
  if (!pref_name || *pref_name == prefs::kWebKitSerifFontFamily) {
    serif_font_size_pixel_ = serif_size_.GetValue();
    serif_font_display_view_->SetFontType(
        UTF8ToWide(serif_name_.GetValue()),
        serif_font_size_pixel_);
  }
  if (!pref_name || *pref_name == prefs::kWebKitSansSerifFontFamily) {
    sans_serif_font_size_pixel_ = sans_serif_size_.GetValue();
    sans_serif_font_display_view_->SetFontType(
        UTF8ToWide(sans_serif_name_.GetValue()),
        sans_serif_font_size_pixel_);
  }
}

void FontsPageView::InitFontLayout() {
  // Fixed width.
  fixed_width_font_display_view_ = new FontDisplayView;
  fixed_width_font_change_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_BUTTON_LABEL)));

  fixed_width_font_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_FIXED_WIDTH_LABEL)));
  fixed_width_font_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Serif font.
  serif_font_display_view_ = new FontDisplayView;
  serif_font_change_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_BUTTON_LABEL)));

  serif_font_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SERIF_LABEL)));
  serif_font_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Sans Serif font.
  sans_serif_font_display_view_ = new FontDisplayView;
  sans_serif_font_change_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_BUTTON_LABEL)));

  sans_serif_font_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_SELECTOR_SANS_SERIF_LABEL)));
  sans_serif_font_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Now add the views.
  using views::GridLayout;
  using views::ColumnSet;

  fonts_contents_ = new views::View;
  GridLayout* layout = new GridLayout(fonts_contents_);
  fonts_contents_->SetLayoutManager(layout);

  const int triple_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(triple_column_view_set_id);

  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Serif font controls.
  layout->StartRow(0, triple_column_view_set_id);
  layout->AddView(serif_font_label_);
  layout->AddView(serif_font_display_view_, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(serif_font_change_page_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Sans serif font controls.
  layout->StartRow(0, triple_column_view_set_id);
  layout->AddView(sans_serif_font_label_);
  layout->AddView(sans_serif_font_display_view_, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(sans_serif_font_change_page_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Fixed-width font controls.
  layout->StartRow(0, triple_column_view_set_id);
  layout->AddView(fixed_width_font_label_);
  layout->AddView(fixed_width_font_display_view_, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(fixed_width_font_change_page_button_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

void FontsPageView::InitEncodingLayout() {
  default_encoding_combobox_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_FONT_LANGUAGE_SETTING_FONT_DEFAULT_ENCODING_SELECTOR_LABEL)));
  default_encoding_combobox_model_.reset(new DefaultEncodingComboboxModel);
  default_encoding_combobox_ = new views::Combobox(
      default_encoding_combobox_model_.get());
  int selected_encoding_index = default_encoding_combobox_model_->
      GetSelectedEncodingIndex(profile());
  default_encoding_combobox_->SetSelectedItem(selected_encoding_index);
  default_encoding_selected_ = default_encoding_combobox_model_->
      GetEncodingCharsetByIndex(selected_encoding_index);
  default_encoding_combobox_->set_listener(this);

  // Now add the views.
  using views::GridLayout;
  using views::ColumnSet;

  encoding_contents_ = new views::View;
  GridLayout* layout = new GridLayout(encoding_contents_);
  encoding_contents_->SetLayoutManager(layout);

  // Double column.
  const int double_column_view_set_id = 2;
  ColumnSet* column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Add Encoding Combobox.
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(default_encoding_combobox_label_);
  layout->AddView(default_encoding_combobox_, 1, 1, GridLayout::FILL,
      GridLayout::CENTER);
}
