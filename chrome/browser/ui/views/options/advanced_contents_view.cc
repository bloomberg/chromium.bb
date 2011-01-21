// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/advanced_contents_view.h"

#include <windows.h>

#include <cryptuiapi.h>
#pragma comment(lib, "cryptui.lib")
#include <shellapi.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <string>

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_callback_factory.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/gears_integration.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_set_observer.h"
#include "chrome/browser/printing/cloud_print/cloud_print_proxy_service.h"
#include "chrome/browser/printing/cloud_print/cloud_print_setup_flow.h"
#include "chrome/browser/printing/cloud_print/cloud_print_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/clear_browsing_data.h"
#include "chrome/browser/ui/views/list_background.h"
#include "chrome/browser/ui/views/options/content_settings_window_view.h"
#include "chrome/browser/ui/views/options/fonts_languages_window_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/canvas_skia.h"
#include "gfx/native_theme_win.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/ssl_config_service_win.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/scroll_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

using views::GridLayout;
using views::ColumnSet;

namespace {

const int kFileIconSize = 16;
const int kFileIconVerticalSpacing = 3;
const int kFileIconHorizontalSpacing = 3;
const int kFileIconTextFieldSpacing = 3;

////////////////////////////////////////////////////////////////////////////////
// FileDisplayArea

class FileDisplayArea : public views::View {
 public:
  FileDisplayArea();
  virtual ~FileDisplayArea();

  void SetFile(const FilePath& file_path);

  // views::View overrides:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();

 protected:
  // views::View overrides:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

 private:
  void Init();

  views::Textfield* text_field_;
  SkColor text_field_background_color_;

  gfx::Rect icon_bounds_;

  bool initialized_;

  static void InitClass();
  static SkBitmap default_folder_icon_;

  DISALLOW_COPY_AND_ASSIGN(FileDisplayArea);
};

// static
SkBitmap FileDisplayArea::default_folder_icon_;

FileDisplayArea::FileDisplayArea()
    : text_field_(new views::Textfield),
      text_field_background_color_(0),
      initialized_(false) {
  InitClass();
}

FileDisplayArea::~FileDisplayArea() {
}

void FileDisplayArea::SetFile(const FilePath& file_path) {
  // Force file path to have LTR directionality.
  if (base::i18n::IsRTL()) {
    string16 localized_file_path;
    base::i18n::WrapPathWithLTRFormatting(file_path, &localized_file_path);
    text_field_->SetText(UTF16ToWide(localized_file_path));
  } else {
    text_field_->SetText(file_path.ToWStringHack());
  }
}

void FileDisplayArea::Paint(gfx::Canvas* canvas) {
  HDC dc = canvas->BeginPlatformPaint();
  RECT rect = { 0, 0, width(), height() };
  gfx::NativeTheme::instance()->PaintTextField(
      dc, EP_EDITTEXT, ETS_READONLY, 0, &rect,
      skia::SkColorToCOLORREF(text_field_background_color_), true, true);
  canvas->EndPlatformPaint();
  // Mirror left point for icon_bounds_ to draw icon in RTL locales correctly.
  canvas->DrawBitmapInt(default_folder_icon_,
                        MirroredLeftPointForRect(icon_bounds_),
                        icon_bounds_.y());
}

void FileDisplayArea::Layout() {
  icon_bounds_.SetRect(kFileIconHorizontalSpacing, kFileIconVerticalSpacing,
                       kFileIconSize, kFileIconSize);
  gfx::Size ps = text_field_->GetPreferredSize();
  text_field_->SetBounds(icon_bounds_.right() + kFileIconTextFieldSpacing,
                         (height() - ps.height()) / 2,
                         width() - icon_bounds_.right() -
                             kFileIconHorizontalSpacing -
                             kFileIconTextFieldSpacing, ps.height());
}

gfx::Size FileDisplayArea::GetPreferredSize() {
  return gfx::Size(kFileIconSize + 2 * kFileIconVerticalSpacing,
                   kFileIconSize + 2 * kFileIconHorizontalSpacing);
}

void FileDisplayArea::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  if (!initialized_ && is_add && GetWidget())
    Init();
}

void FileDisplayArea::Init() {
  initialized_ = true;
  AddChildView(text_field_);
  text_field_background_color_ =
      gfx::NativeTheme::instance()->GetThemeColorWithDefault(
          gfx::NativeTheme::TEXTFIELD, EP_EDITTEXT, ETS_READONLY,
          TMT_FILLCOLOR, COLOR_3DFACE);
  text_field_->SetReadOnly(true);
  text_field_->RemoveBorder();
  text_field_->SetBackgroundColor(text_field_background_color_);
}

// static
void FileDisplayArea::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    // We'd prefer to use base::i18n::IsRTL() to perform the RTL
    // environment check, but it's nonstatic, so, instead, we check whether the
    // locale is RTL.
    bool ui_is_rtl = base::i18n::IsRTL();
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_folder_icon_ = *rb.GetBitmapNamed(ui_is_rtl ?
                                              IDR_FOLDER_CLOSED_RTL :
                                              IDR_FOLDER_CLOSED);
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection
//  A convenience view for grouping advanced options together into related
//  sections.
//
class AdvancedSection : public OptionsPageView {
 public:
  AdvancedSection(Profile* profile, const std::wstring& title);
  virtual ~AdvancedSection() {}

  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 protected:
  // Convenience helpers to add different kinds of ColumnSets for specific
  // types of layout.
  void AddWrappingColumnSet(views::GridLayout* layout, int id);
  void AddDependentTwoColumnSet(views::GridLayout* layout, int id);
  void AddTwoColumnSet(views::GridLayout* layout, int id);
  void AddIndentedColumnSet(views::GridLayout* layout, int id);

  // Convenience helpers for adding controls to specific layouts in an
  // aesthetically pleasing way.
  void AddWrappingCheckboxRow(views::GridLayout* layout,
                              views::Checkbox* checkbox,
                              int id,
                              bool related_follows);
  void AddWrappingLabelRow(views::GridLayout* layout,
                           views::Label* label,
                           int id,
                           bool related_follows);
  void AddLabeledTwoColumnRow(views::GridLayout* layout,
                              views::Label* label,
                              views::View* control,
                              bool control_stretches,
                              int id,
                              bool related_follows);
  void AddTwoColumnRow(views::GridLayout* layout,
                       views::View* first,
                       views::View* second,
                       bool control_stretches,  // Whether or not the control
                                                // expands to fill the width.
                       int id,
                       int trailing_space);
  void AddLeadingControl(views::GridLayout* layout,
                         views::View* control,
                         int id,
                         bool related_follows);
  void AddIndentedControl(views::GridLayout* layout,
                          views::View* control,
                          int id,
                          bool related_follows);
  void AddSpacing(views::GridLayout* layout, bool related_follows);

  // OptionsPageView overrides:
  virtual void InitControlLayout();

  // The View that contains the contents of the section.
  views::View* contents_;

 private:
  // The section title.
  views::Label* title_label_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedSection);
};

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, public:

AdvancedSection::AdvancedSection(Profile* profile,
                                 const std::wstring& title)
    : contents_(NULL),
      title_label_(new views::Label(title)),
      OptionsPageView(profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label_->SetFont(title_font);

  SkColor title_color = gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetColor(title_color);
}

void AdvancedSection::DidChangeBounds(const gfx::Rect& previous,
                                      const gfx::Rect& current) {
  Layout();
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, protected:

void AdvancedSection::AddWrappingColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddDependentTwoColumnSet(views::GridLayout* layout,
                                               int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, views::Checkbox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
}

void AdvancedSection::AddTwoColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddIndentedColumnSet(views::GridLayout* layout, int id) {
  ColumnSet* column_set = layout->AddColumnSet(id);
  column_set->AddPaddingColumn(0, views::Checkbox::GetTextIndent());
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
}

void AdvancedSection::AddWrappingCheckboxRow(views::GridLayout* layout,
                                             views::Checkbox* checkbox,
                                             int id,
                                             bool related_follows) {
  checkbox->SetMultiLine(true);
  layout->StartRow(0, id);
  layout->AddView(checkbox);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddWrappingLabelRow(views::GridLayout* layout,
                                          views::Label* label,
                                          int id,
                                          bool related_follows) {
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->StartRow(0, id);
  layout->AddView(label);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddLabeledTwoColumnRow(views::GridLayout* layout,
                                             views::Label* label,
                                             views::View* control,
                                             bool control_stretches,
                                             int id,
                                             bool related_follows) {
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddTwoColumnRow(layout, label, control, control_stretches, id,
      related_follows ?
          kRelatedControlVerticalSpacing : kUnrelatedControlVerticalSpacing);
}

void AdvancedSection::AddTwoColumnRow(views::GridLayout* layout,
                                      views::View* first,
                                      views::View* second,
                                      bool control_stretches,
                                      int id,
                                      int trailing_space) {
  layout->StartRow(0, id);
  layout->AddView(first);
  if (control_stretches) {
    layout->AddView(second);
  } else {
    layout->AddView(second, 1, 1, views::GridLayout::LEADING,
                    views::GridLayout::CENTER);
  }
  layout->AddPaddingRow(0, trailing_space);
}

void AdvancedSection::AddLeadingControl(views::GridLayout* layout,
                                        views::View* control,
                                        int id,
                                        bool related_follows) {
  layout->StartRow(0, id);
  layout->AddView(control, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
  AddSpacing(layout, related_follows);
}

void AdvancedSection::AddSpacing(views::GridLayout* layout,
                                 bool related_follows) {
  layout->AddPaddingRow(0, related_follows ? kRelatedControlVerticalSpacing
                                           : kUnrelatedControlVerticalSpacing);
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedSection, OptionsPageView overrides:

void AdvancedSection::InitControlLayout() {
  contents_ = new views::View;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);
  const int inset_column_layout_id = 1;
  column_set = layout->AddColumnSet(inset_column_layout_id);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_layout_id);
  layout->AddView(title_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, inset_column_layout_id);
  layout->AddView(contents_);
}

////////////////////////////////////////////////////////////////////////////////
// PrivacySection

class PrivacySection : public AdvancedSection,
                       public views::ButtonListener,
                       public views::LinkController {
 public:
  explicit PrivacySection(Profile* profile);
  virtual ~PrivacySection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // Controls for this section:
  views::NativeButton* content_settings_button_;
  views::NativeButton* clear_data_button_;
  views::Label* section_description_label_;
  views::Checkbox* enable_link_doctor_checkbox_;
  views::Checkbox* enable_suggest_checkbox_;
  views::Checkbox* enable_dns_prefetching_checkbox_;
  views::Checkbox* enable_safe_browsing_checkbox_;
  views::Checkbox* reporting_enabled_checkbox_;
  views::Link* learn_more_link_;

  // Preferences for this section:
  BooleanPrefMember alternate_error_pages_;
  BooleanPrefMember use_suggest_;
  BooleanPrefMember dns_prefetch_enabled_;
  BooleanPrefMember safe_browsing_;
  BooleanPrefMember enable_metrics_recording_;

  void ResolveMetricsReportingEnabled();

  DISALLOW_COPY_AND_ASSIGN(PrivacySection);
};

PrivacySection::PrivacySection(Profile* profile)
    : content_settings_button_(NULL),
      clear_data_button_(NULL),
      section_description_label_(NULL),
      enable_link_doctor_checkbox_(NULL),
      enable_suggest_checkbox_(NULL),
      enable_dns_prefetching_checkbox_(NULL),
      enable_safe_browsing_checkbox_(NULL),
      reporting_enabled_checkbox_(NULL),
      learn_more_link_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY))) {
}

void PrivacySection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == enable_link_doctor_checkbox_) {
    bool enabled = enable_link_doctor_checkbox_->checked();
    UserMetricsRecordAction(UserMetricsAction(enabled ?
                                "Options_LinkDoctorCheckbox_Enable" :
                                "Options_LinkDoctorCheckbox_Disable"),
                            profile()->GetPrefs());
    alternate_error_pages_.SetValue(enabled);
  } else if (sender == enable_suggest_checkbox_) {
    bool enabled = enable_suggest_checkbox_->checked();
    UserMetricsRecordAction(UserMetricsAction(enabled ?
                                "Options_UseSuggestCheckbox_Enable" :
                                "Options_UseSuggestCheckbox_Disable"),
                            profile()->GetPrefs());
    use_suggest_.SetValue(enabled);
  } else if (sender == enable_dns_prefetching_checkbox_) {
    bool enabled = enable_dns_prefetching_checkbox_->checked();
    UserMetricsRecordAction(UserMetricsAction(enabled ?
                                "Options_DnsPrefetchCheckbox_Enable" :
                                "Options_DnsPrefetchCheckbox_Disable"),
                            profile()->GetPrefs());
    dns_prefetch_enabled_.SetValue(enabled);
  } else if (sender == enable_safe_browsing_checkbox_) {
    bool enabled = enable_safe_browsing_checkbox_->checked();
    UserMetricsRecordAction(UserMetricsAction(enabled ?
                                "Options_SafeBrowsingCheckbox_Enable" :
                                "Options_SafeBrowsingCheckbox_Disable"),
                            profile()->GetPrefs());
    safe_browsing_.SetValue(enabled);
    SafeBrowsingService* safe_browsing_service =
        g_browser_process->resource_dispatcher_host()->safe_browsing_service();
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        safe_browsing_service, &SafeBrowsingService::OnEnable, enabled));
  } else if (reporting_enabled_checkbox_ &&
             (sender == reporting_enabled_checkbox_)) {
    bool enabled = reporting_enabled_checkbox_->checked();
    UserMetricsRecordAction(UserMetricsAction(enabled ?
                                "Options_MetricsReportingCheckbox_Enable" :
                                "Options_MetricsReportingCheckbox_Disable"),
                            profile()->GetPrefs());
    ResolveMetricsReportingEnabled();
    enable_metrics_recording_.SetValue(enabled);
  } else if (sender == content_settings_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ContentSettings"), NULL);
    browser::ShowContentSettingsWindow(GetWindow()->GetNativeWindow(),
        CONTENT_SETTINGS_TYPE_DEFAULT, profile());
  } else if (sender == clear_data_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ClearData"), NULL);
    views::Window::CreateChromeWindow(
        GetWindow()->GetNativeWindow(),
        gfx::Rect(),
        new ClearBrowsingDataView(profile()))->Show();
  }
}

void PrivacySection::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == learn_more_link_);
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kPrivacyLearnMoreURL));
  browser::ShowOptionsURL(profile(), url);
}

void PrivacySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  content_settings_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_PRIVACY_CONTENT_SETTINGS_BUTTON)));
  clear_data_button_ =  new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_PRIVACY_CLEAR_DATA_BUTTON)));
  section_description_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_DISABLE_SERVICES)));
  enable_link_doctor_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_LINKDOCTOR_PREF)));
  enable_link_doctor_checkbox_->set_listener(this);
  enable_suggest_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SUGGEST_PREF)));
  enable_suggest_checkbox_->set_listener(this);
  enable_dns_prefetching_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_NETWORK_DNS_PREFETCH_ENABLED_DESCRIPTION)));
  enable_dns_prefetching_checkbox_->set_listener(this);
  enable_safe_browsing_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_SAFEBROWSING_ENABLEPROTECTION)));
  enable_safe_browsing_checkbox_->set_listener(this);
#if defined(GOOGLE_CHROME_BUILD)
  reporting_enabled_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_ENABLE_LOGGING)));
  reporting_enabled_checkbox_->SetMultiLine(true);
  reporting_enabled_checkbox_->set_listener(this);
  reporting_enabled_checkbox_->SetVisible(true);
#endif
  learn_more_link_ = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
  learn_more_link_->SetController(this);

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int leading_column_set_id = 0;
  AddTwoColumnSet(layout, leading_column_set_id);
  const int single_column_view_set_id = 1;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int dependent_labeled_field_set_id = 2;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int indented_view_set_id = 3;
  AddIndentedColumnSet(layout, indented_view_set_id);
  const int indented_column_set_id = 4;
  AddIndentedColumnSet(layout, indented_column_set_id);

  AddTwoColumnRow(layout, content_settings_button_, clear_data_button_, false,
                  leading_column_set_id, kUnrelatedControlLargeVerticalSpacing);

  // The description label at the top and label.
  section_description_label_->SetMultiLine(true);
  AddWrappingLabelRow(layout, section_description_label_,
                       single_column_view_set_id, true);
  // Learn more link.
  AddLeadingControl(layout, learn_more_link_,
                    single_column_view_set_id, true);

  // Link doctor.
  AddWrappingCheckboxRow(layout, enable_link_doctor_checkbox_,
                         indented_view_set_id, true);
  // Use Suggest service.
  AddWrappingCheckboxRow(layout, enable_suggest_checkbox_,
                         indented_view_set_id, true);
  // DNS pre-fetching.
  AddWrappingCheckboxRow(layout, enable_dns_prefetching_checkbox_,
                         indented_view_set_id, true);
  // Safe browsing controls.
  AddWrappingCheckboxRow(layout, enable_safe_browsing_checkbox_,
                         indented_view_set_id,
                         reporting_enabled_checkbox_ != NULL);
  // The "Help make Google Chrome better" checkbox.
  if (reporting_enabled_checkbox_) {
    AddWrappingCheckboxRow(layout, reporting_enabled_checkbox_,
                           indented_view_set_id, false);
  }

  // Init member prefs so we can update the controls if prefs change.
  alternate_error_pages_.Init(prefs::kAlternateErrorPagesEnabled,
                              profile()->GetPrefs(), this);
  use_suggest_.Init(prefs::kSearchSuggestEnabled,
                    profile()->GetPrefs(), this);
  dns_prefetch_enabled_.Init(prefs::kDnsPrefetchingEnabled,
                             profile()->GetPrefs(), this);
  safe_browsing_.Init(prefs::kSafeBrowsingEnabled, profile()->GetPrefs(), this);
  enable_metrics_recording_.Init(prefs::kMetricsReportingEnabled,
                                 g_browser_process->local_state(), this);
}

void PrivacySection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kAlternateErrorPagesEnabled) {
    enable_link_doctor_checkbox_->SetEnabled(
        !alternate_error_pages_.IsManaged());
    enable_link_doctor_checkbox_->SetChecked(
        alternate_error_pages_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kSearchSuggestEnabled) {
    enable_suggest_checkbox_->SetEnabled(!use_suggest_.IsManaged());
    enable_suggest_checkbox_->SetChecked(use_suggest_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kDnsPrefetchingEnabled) {
    enable_dns_prefetching_checkbox_->SetEnabled(
        !dns_prefetch_enabled_.IsManaged());
    bool enabled = dns_prefetch_enabled_.GetValue();
    enable_dns_prefetching_checkbox_->SetChecked(enabled);
  }
  if (!pref_name || *pref_name == prefs::kSafeBrowsingEnabled) {
    enable_safe_browsing_checkbox_->SetEnabled(!safe_browsing_.IsManaged());
    enable_safe_browsing_checkbox_->SetChecked(safe_browsing_.GetValue());
  }
  if (reporting_enabled_checkbox_ &&
      (!pref_name || *pref_name == prefs::kMetricsReportingEnabled)) {
    reporting_enabled_checkbox_->SetEnabled(
        !enable_metrics_recording_.IsManaged());
    reporting_enabled_checkbox_->SetChecked(
        enable_metrics_recording_.GetValue());
    ResolveMetricsReportingEnabled();
  }
}

void PrivacySection::ResolveMetricsReportingEnabled() {
  DCHECK(reporting_enabled_checkbox_);
  bool enabled = reporting_enabled_checkbox_->checked();

  enabled = OptionsUtil::ResolveMetricsReportingEnabled(enabled);

  reporting_enabled_checkbox_->SetChecked(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentSection

class WebContentSection : public AdvancedSection,
                          public views::ButtonListener {
 public:
  explicit WebContentSection(Profile* profile);
  virtual ~WebContentSection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();

 private:
  // Controls for this section:
  views::Label* fonts_and_languages_label_;
  views::NativeButton* change_content_fonts_button_;
  views::Label* gears_label_;
  views::NativeButton* gears_settings_button_;

  DISALLOW_COPY_AND_ASSIGN(WebContentSection);
};

WebContentSection::WebContentSection(Profile* profile)
    : fonts_and_languages_label_(NULL),
      change_content_fonts_button_(NULL),
      gears_label_(NULL),
      gears_settings_button_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_CONTENT))) {
}

void WebContentSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == gears_settings_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_GearsSettings"), NULL);
    GearsSettingsPressed(GetAncestor(GetWidget()->GetNativeView(), GA_ROOT));
  } else if (sender == change_content_fonts_button_) {
    views::Window::CreateChromeWindow(
        GetWindow()->GetNativeWindow(),
        gfx::Rect(),
        new FontsLanguagesWindowView(profile()))->Show();
  }
}

void WebContentSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  if (!base::i18n::IsRTL()) {
    gears_label_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME)));
  } else {
    // Add an RTL mark so that
    // ":" in "Google Gears:" in Hebrew Chrome is displayed left-most.
    std::wstring gearssetting_group_name = UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_OPTIONS_GEARSSETTINGS_GROUP_NAME));
    gearssetting_group_name.push_back(
        static_cast<wchar_t>(base::i18n::kRightToLeftMark));
    gears_label_ = new views::Label(gearssetting_group_name);
  }
  gears_settings_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_GEARSSETTINGS_CONFIGUREGEARS_BUTTON)));
  fonts_and_languages_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_FONTSETTINGS_INFO)));

  change_content_fonts_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_FONTSETTINGS_CONFIGUREFONTS_BUTTON)));

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int indented_column_set_id = 1;
  AddIndentedColumnSet(layout, indented_column_set_id);
  const int single_double_column_set = 2;
  AddTwoColumnSet(layout, single_double_column_set);

  // Fonts and Languages.
  AddWrappingLabelRow(layout, fonts_and_languages_label_,
                      single_column_view_set_id,
                      true);
  AddLeadingControl(layout, change_content_fonts_button_,
                    indented_column_set_id,
                    false);

  // Gears.
  AddLabeledTwoColumnRow(layout, gears_label_, gears_settings_button_, false,
                         single_double_column_set, false);
}

////////////////////////////////////////////////////////////////////////////////
// SecuritySection

class SecuritySection : public AdvancedSection,
                        public views::ButtonListener {
 public:
  explicit SecuritySection(Profile* profile);
  virtual ~SecuritySection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // Controls for this section:
  views::Label* ssl_info_label_;
  views::Checkbox* enable_ssl3_checkbox_;
  views::Checkbox* enable_tls1_checkbox_;
  views::Checkbox* check_for_cert_revocation_checkbox_;
  views::Label* manage_certificates_label_;
  views::NativeButton* manage_certificates_button_;

  DISALLOW_COPY_AND_ASSIGN(SecuritySection);
};

SecuritySection::SecuritySection(Profile* profile)
    : ssl_info_label_(NULL),
      enable_ssl3_checkbox_(NULL),
      enable_tls1_checkbox_(NULL),
      check_for_cert_revocation_checkbox_(NULL),
      manage_certificates_label_(NULL),
      manage_certificates_button_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_SECURITY))) {
}

void SecuritySection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == enable_ssl3_checkbox_) {
    bool enabled = enable_ssl3_checkbox_->checked();
    if (enabled) {
      UserMetricsRecordAction(UserMetricsAction("Options_SSL3_Enable"), NULL);
    } else {
      UserMetricsRecordAction(UserMetricsAction("Options_SSL3_Disable"), NULL);
    }
    net::SSLConfigServiceWin::SetSSL3Enabled(enabled);
  } else if (sender == enable_tls1_checkbox_) {
    bool enabled = enable_tls1_checkbox_->checked();
    if (enabled) {
      UserMetricsRecordAction(UserMetricsAction("Options_TLS1_Enable"), NULL);
    } else {
      UserMetricsRecordAction(UserMetricsAction("Options_TLS1_Disable"), NULL);
    }
    net::SSLConfigServiceWin::SetTLS1Enabled(enabled);
  } else if (sender == check_for_cert_revocation_checkbox_) {
    bool enabled = check_for_cert_revocation_checkbox_->checked();
    if (enabled) {
      UserMetricsRecordAction(
          UserMetricsAction("Options_CheckCertRevocation_Enable"), NULL);
    } else {
      UserMetricsRecordAction(
          UserMetricsAction("Options_CheckCertRevocation_Disable"), NULL);
    }
    net::SSLConfigServiceWin::SetRevCheckingEnabled(enabled);
  } else if (sender == manage_certificates_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ManagerCerts"), NULL);
    CRYPTUI_CERT_MGR_STRUCT cert_mgr = { 0 };
    cert_mgr.dwSize = sizeof(CRYPTUI_CERT_MGR_STRUCT);
    cert_mgr.hwndParent = GetWindow()->GetNativeWindow();
    ::CryptUIDlgCertMgr(&cert_mgr);
  }
}

void SecuritySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  ssl_info_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_GROUP_DESCRIPTION)));
  enable_ssl3_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USESSL3)));
  enable_ssl3_checkbox_->set_listener(this);
  enable_tls1_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_USETLS1)));
  enable_tls1_checkbox_->set_listener(this);
  check_for_cert_revocation_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_SSL_CHECKREVOCATION)));
  check_for_cert_revocation_checkbox_->set_listener(this);
  manage_certificates_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_LABEL)));
  manage_certificates_button_ = new views::NativeButton(
      this,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_CERTIFICATES_MANAGE_BUTTON)));

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int dependent_labeled_field_set_id = 1;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int double_column_view_set_id = 2;
  AddTwoColumnSet(layout, double_column_view_set_id);
  const int indented_column_set_id = 3;
  AddIndentedColumnSet(layout, indented_column_set_id);
  const int indented_view_set_id = 4;
  AddIndentedColumnSet(layout, indented_view_set_id);

  // SSL connection controls and Certificates.
  AddWrappingLabelRow(layout, manage_certificates_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, manage_certificates_button_,
                    indented_column_set_id, false);
  AddWrappingLabelRow(layout, ssl_info_label_, single_column_view_set_id,
                      true);
  AddWrappingCheckboxRow(layout, enable_ssl3_checkbox_,
                         indented_column_set_id, true);
  AddWrappingCheckboxRow(layout, enable_tls1_checkbox_,
                         indented_column_set_id, true);
  AddWrappingCheckboxRow(layout, check_for_cert_revocation_checkbox_,
                         indented_column_set_id, false);
}

// This method is called with a null pref_name when the dialog is initialized.
void SecuritySection::NotifyPrefChanged(const std::string* pref_name) {
  // These SSL options are system settings and stored in the OS.
  if (!pref_name) {
    net::SSLConfig config;
    if (net::SSLConfigServiceWin::GetSSLConfigNow(&config)) {
      enable_ssl3_checkbox_->SetChecked(config.ssl3_enabled);
      enable_tls1_checkbox_->SetChecked(config.tls1_enabled);
      check_for_cert_revocation_checkbox_->SetChecked(
          config.rev_checking_enabled);
    } else {
      enable_ssl3_checkbox_->SetEnabled(false);
      enable_tls1_checkbox_->SetEnabled(false);
      check_for_cert_revocation_checkbox_->SetEnabled(false);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkSection

// A helper method that opens the Internet Options control panel dialog with
// the Connections tab selected.
class OpenConnectionDialogTask : public Task {
 public:
  OpenConnectionDialogTask() {}

  virtual void Run() {
    // Using rundll32 seems better than LaunchConnectionDialog which causes a
    // new dialog to be made for each call.  rundll32 uses the same global
    // dialog and it seems to share with the shortcut in control panel.
    FilePath rundll32;
    PathService::Get(base::DIR_SYSTEM, &rundll32);
    rundll32 = rundll32.AppendASCII("rundll32.exe");

    FilePath shell32dll;
    PathService::Get(base::DIR_SYSTEM, &shell32dll);
    shell32dll = shell32dll.AppendASCII("shell32.dll");

    FilePath inetcpl;
    PathService::Get(base::DIR_SYSTEM, &inetcpl);
    inetcpl = inetcpl.AppendASCII("inetcpl.cpl,,4");

    std::wstring args(shell32dll.value());
    args.append(L",Control_RunDLL ");
    args.append(inetcpl.value());

    ShellExecute(NULL, L"open", rundll32.value().c_str(), args.c_str(), NULL,
        SW_SHOWNORMAL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenConnectionDialogTask);
};

class NetworkSection : public AdvancedSection,
                       public views::ButtonListener {
 public:
  explicit NetworkSection(Profile* profile);
  virtual ~NetworkSection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // Controls for this section:
  views::Label* change_proxies_label_;
  views::NativeButton* change_proxies_button_;

  // Tracks the proxy preferences.
  scoped_ptr<PrefSetObserver> proxy_prefs_;

  DISALLOW_COPY_AND_ASSIGN(NetworkSection);
};

NetworkSection::NetworkSection(Profile* profile)
    : change_proxies_label_(NULL),
      change_proxies_button_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_NETWORK))) {
}

void NetworkSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == change_proxies_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ChangeProxies"), NULL);
    base::Thread* thread = g_browser_process->file_thread();
    DCHECK(thread);
    thread->message_loop()->PostTask(FROM_HERE, new OpenConnectionDialogTask);
  }
}

void NetworkSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  change_proxies_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_LABEL)));
  change_proxies_button_ = new views::NativeButton(
      this, UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_PROXIES_CONFIGURE_BUTTON)));

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int indented_view_set_id = 1;
  AddIndentedColumnSet(layout, indented_view_set_id);
  const int dependent_labeled_field_set_id = 2;
  AddDependentTwoColumnSet(layout, dependent_labeled_field_set_id);
  const int dns_set_id = 3;
  AddDependentTwoColumnSet(layout, dns_set_id);

  // Proxy settings.
  AddWrappingLabelRow(layout, change_proxies_label_, single_column_view_set_id,
                      true);
  AddLeadingControl(layout, change_proxies_button_, indented_view_set_id,
                    false);

  proxy_prefs_.reset(PrefSetObserver::CreateProxyPrefSetObserver(
      profile()->GetPrefs(), this));
  NotifyPrefChanged(NULL);
}

void NetworkSection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || proxy_prefs_->IsObserved(*pref_name)) {
    change_proxies_button_->SetEnabled(!proxy_prefs_->IsManaged());
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DownloadSection

class DownloadSection : public AdvancedSection,
                        public views::ButtonListener,
                        public SelectFileDialog::Listener {
 public:
  explicit DownloadSection(Profile* profile);
  virtual ~DownloadSection() {
    select_file_dialog_->ListenerDestroyed();
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // SelectFileDialog::Listener implementation.
  virtual void FileSelected(const FilePath& path, int index, void* params);

  // OptionsPageView implementation.
  virtual bool CanClose() const;

 protected:
  // OptionsPageView overrides.
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // Controls for this section.
  views::Label* download_file_location_label_;
  FileDisplayArea* download_default_download_location_display_;
  views::NativeButton* download_browse_button_;
  views::Checkbox* download_ask_for_save_location_checkbox_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  views::Label* reset_file_handlers_label_;
  views::NativeButton* reset_file_handlers_button_;

  // Pref members.
  FilePathPrefMember default_download_location_;
  BooleanPrefMember ask_for_save_location_;

  // Updates the directory displayed in the default download location view with
  // the current value of the pref.
  void UpdateDownloadDirectoryDisplay();

  StringPrefMember auto_open_files_;

  DISALLOW_COPY_AND_ASSIGN(DownloadSection);
};

DownloadSection::DownloadSection(Profile* profile)
    : download_file_location_label_(NULL),
      download_default_download_location_display_(NULL),
      download_browse_button_(NULL),
      download_ask_for_save_location_checkbox_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          select_file_dialog_(SelectFileDialog::Create(this))),
      reset_file_handlers_label_(NULL),
      reset_file_handlers_button_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_GROUP_NAME))) {
}

void DownloadSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == download_browse_button_) {
    const std::wstring dialog_title = UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE));
    select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                    dialog_title,
                                    profile()->GetPrefs()->GetFilePath(
                                        prefs::kDownloadDefaultDirectory),
                                    NULL, 0, std::wstring(),
                                    GetWindow()->GetNativeWindow(),
                                    NULL);
  } else if (sender == download_ask_for_save_location_checkbox_) {
    bool enabled = download_ask_for_save_location_checkbox_->checked();
    if (enabled) {
      UserMetricsRecordAction(
                UserMetricsAction("Options_AskForSaveLocation_Enable"),
                profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(
                UserMetricsAction("Options_AskForSaveLocation_Disable"),
                profile()->GetPrefs());
    }
    ask_for_save_location_.SetValue(enabled);
  } else if (sender == reset_file_handlers_button_) {
    profile()->GetDownloadManager()->download_prefs()->ResetAutoOpen();
    UserMetricsRecordAction(UserMetricsAction("Options_ResetAutoOpenFiles"),
                            profile()->GetPrefs());
  }
}

void DownloadSection::FileSelected(const FilePath& path,
                                   int index, void* params) {
  UserMetricsRecordAction(UserMetricsAction("Options_SetDownloadDirectory"),
                          profile()->GetPrefs());
  default_download_location_.SetValue(path);
  // We need to call this manually here since because we're setting the value
  // through the pref member which avoids notifying the listener that set the
  // value.
  UpdateDownloadDirectoryDisplay();
}

bool DownloadSection::CanClose() const {
  return !select_file_dialog_->IsRunning(GetWindow()->GetNativeWindow());
}

void DownloadSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  // Layout the download components.
  download_file_location_label_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_TITLE)));
  download_default_download_location_display_ = new FileDisplayArea;
  download_browse_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_BROWSE_BUTTON)));

  download_ask_for_save_location_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_DOWNLOADLOCATION_ASKFORSAVELOCATION)));
  download_ask_for_save_location_checkbox_->set_listener(this);
  download_ask_for_save_location_checkbox_->SetMultiLine(true);
  reset_file_handlers_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_AUTOOPENFILETYPES_INFO)));
  reset_file_handlers_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_AUTOOPENFILETYPES_RESETTODEFAULT)));

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  // Download location label.
  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  AddWrappingLabelRow(layout, download_file_location_label_,
      single_column_view_set_id, true);

  // Download location control.
  const int double_column_view_set_id = 1;
  ColumnSet* column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kUnrelatedControlHorizontalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(download_default_download_location_display_, 1, 1,
                  GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(download_browse_button_);
  AddSpacing(layout, true);

  // Save location checkbox layout.
  const int indented_view_set_id = 2;
  AddIndentedColumnSet(layout, indented_view_set_id);
  AddWrappingCheckboxRow(layout, download_ask_for_save_location_checkbox_,
                         indented_view_set_id, false);

  // Reset file handlers layout.
  AddWrappingLabelRow(layout, reset_file_handlers_label_,
                      single_column_view_set_id, true);
  AddLeadingControl(layout, reset_file_handlers_button_,
                    indented_view_set_id,
                    false);

  // Init member prefs so we can update the controls if prefs change.
  default_download_location_.Init(prefs::kDownloadDefaultDirectory,
                                  profile()->GetPrefs(), this);
  ask_for_save_location_.Init(prefs::kPromptForDownload,
                              profile()->GetPrefs(), this);
  auto_open_files_.Init(prefs::kDownloadExtensionsToOpen, profile()->GetPrefs(),
                        this);
}

void DownloadSection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kDownloadDefaultDirectory)
    UpdateDownloadDirectoryDisplay();

  if (!pref_name || *pref_name == prefs::kPromptForDownload) {
    download_ask_for_save_location_checkbox_->SetChecked(
        ask_for_save_location_.GetValue());
  }

  if (!pref_name || *pref_name == prefs::kDownloadExtensionsToOpen) {
    bool enabled =
        profile()->GetDownloadManager()->download_prefs()->IsAutoOpenUsed();
    reset_file_handlers_label_->SetEnabled(enabled);
    reset_file_handlers_button_->SetEnabled(enabled);
  }
}

void DownloadSection::UpdateDownloadDirectoryDisplay() {
  download_default_download_location_display_->SetFile(
      default_download_location_.GetValue());
}

////////////////////////////////////////////////////////////////////////////////
// TranslateSection

class TranslateSection : public AdvancedSection,
                         public views::ButtonListener {
 public:
  explicit TranslateSection(Profile* profile);
  virtual ~TranslateSection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  // Control for this section:
  views::Checkbox* enable_translate_checkbox_;

  // Preferences for this section:
  BooleanPrefMember enable_translate_;

  DISALLOW_COPY_AND_ASSIGN(TranslateSection);
};

TranslateSection::TranslateSection(Profile* profile)
    : enable_translate_checkbox_(NULL),
      AdvancedSection(profile, UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_ADVANCED_SECTION_TITLE_TRANSLATE))) {
}

void TranslateSection::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  DCHECK(sender == enable_translate_checkbox_);
  bool enabled = enable_translate_checkbox_->checked();
  UserMetricsRecordAction(enabled ?
                          UserMetricsAction("Options_Translate_Enable") :
                          UserMetricsAction("Options_Translate_Disable"),
                          profile()->GetPrefs());
  enable_translate_.SetValue(enabled);
}

void TranslateSection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  AddIndentedColumnSet(layout, 0);

  enable_translate_checkbox_ = new views::Checkbox(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_TRANSLATE_ENABLE_TRANSLATE)));
  enable_translate_checkbox_->set_listener(this);
  AddWrappingCheckboxRow(layout, enable_translate_checkbox_, 0, false);

  // Init member pref so we can update the controls if prefs change.
  enable_translate_.Init(prefs::kEnableTranslate, profile()->GetPrefs(), this);
}

void TranslateSection::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kEnableTranslate)
    enable_translate_checkbox_->SetChecked(enable_translate_.GetValue());
}

////////////////////////////////////////////////////////////////////////////////
// CloudPrintProxySection

class CloudPrintProxySection : public AdvancedSection,
                               public views::ButtonListener,
                               public CloudPrintSetupFlow::Delegate {
 public:
  explicit CloudPrintProxySection(Profile* profile);
  virtual ~CloudPrintProxySection() {}

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // CloudPrintSetupFlow::Delegate implementation.
  virtual void OnDialogClosed();

 protected:
  // OptionsPageView overrides:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::string* pref_name);

 private:
  bool Enabled() const;

  // Controls for this section:
  views::Label* section_description_label_;
  views::NativeButton* enable_disable_button_;
  views::NativeButton* manage_printer_button_;

  // Preferences we tie things to.
  StringPrefMember cloud_print_proxy_email_;

  base::ScopedCallbackFactory<CloudPrintProxySection> factory_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxySection);
};

CloudPrintProxySection::CloudPrintProxySection(Profile* profile)
    : section_description_label_(NULL),
      enable_disable_button_(NULL),
      manage_printer_button_(NULL),
      factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      AdvancedSection(profile,
                      UTF16ToWide(l10n_util::GetStringUTF16(
                          IDS_OPTIONS_ADVANCED_SECTION_TITLE_CLOUD_PRINT))) {
}

void CloudPrintProxySection::ButtonPressed(views::Button* sender,
                                           const views::Event& event) {
  if (sender == enable_disable_button_) {
    if (Enabled()) {
      // Enabled, we must be the disable button.
      UserMetricsRecordAction(
          UserMetricsAction("Options_DisableCloudPrintProxy"), NULL);
      profile()->GetCloudPrintProxyService()->DisableForUser();
    } else {
      UserMetricsRecordAction(
          UserMetricsAction("Options_EnableCloudPrintProxy"), NULL);
      // We open a new browser window so the Options dialog doesn't
      // get lost behind other windows.
      enable_disable_button_->SetEnabled(false);
      enable_disable_button_->SetLabel(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLING_BUTTON)));
      enable_disable_button_->InvalidateLayout();
      Layout();
      CloudPrintSetupFlow::OpenDialog(profile(), this,
                                      GetWindow()->GetNativeWindow());
    }
  } else if (sender == manage_printer_button_) {
    UserMetricsRecordAction(
        UserMetricsAction("Options_ManageCloudPrinters"), NULL);
    browser::ShowOptionsURL(
        profile(),
        CloudPrintURL(profile()).GetCloudPrintServiceManageURL());
  }
}

void CloudPrintProxySection::OnDialogClosed() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enable_disable_button_->SetEnabled(true);
  // If the dialog is canceled, the preference won't change, and so we
  // have to revert the button text back to the disabled state.
  if (!Enabled()) {
    enable_disable_button_->SetLabel(UTF16ToWide(l10n_util::GetStringUTF16(
        IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_BUTTON)));
    enable_disable_button_->InvalidateLayout();
    Layout();
  }
}

void CloudPrintProxySection::InitControlLayout() {
  AdvancedSection::InitControlLayout();

  section_description_label_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_LABEL)));
  enable_disable_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_BUTTON)));
  manage_printer_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_MANAGE_BUTTON)));

  GridLayout* layout = new GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  AddWrappingColumnSet(layout, single_column_view_set_id);
  const int control_view_set_id = 1;
  AddDependentTwoColumnSet(layout, control_view_set_id);

  // The description label at the top and label.
  section_description_label_->SetMultiLine(true);
  AddWrappingLabelRow(layout, section_description_label_,
                       single_column_view_set_id, true);

  // The enable / disable button and manage button.
  AddTwoColumnRow(layout, enable_disable_button_, manage_printer_button_, false,
                  control_view_set_id, kRelatedControlVerticalSpacing);

  // Attach the preferences so we can flip things appropriately.
  cloud_print_proxy_email_.Init(prefs::kCloudPrintEmail,
                                profile()->GetPrefs(), this);

  // Start the UI off in the state we think it should be in.
  std::string pref_string(prefs::kCloudPrintEmail);
  NotifyPrefChanged(&pref_string);

  // Kick off a task to ask the background service what the real
  // answer is.
  profile()->GetCloudPrintProxyService()->RefreshStatusFromService();
}

void CloudPrintProxySection::NotifyPrefChanged(const std::string* pref_name) {
  if (pref_name == NULL)
    return;

  if (*pref_name == prefs::kCloudPrintEmail) {
    if (Enabled()) {
      std::string email;
      if (profile()->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail))
        email = profile()->GetPrefs()->GetString(prefs::kCloudPrintEmail);

      section_description_label_->SetText(UTF16ToWide(
          l10n_util::GetStringFUTF16(
              IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_LABEL,
              UTF8ToUTF16(email))));
      enable_disable_button_->SetLabel(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_ENABLED_BUTTON)));
      enable_disable_button_->InvalidateLayout();
      manage_printer_button_->SetVisible(true);
      manage_printer_button_->InvalidateLayout();
    } else {
      section_description_label_->SetText(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_LABEL)));
      enable_disable_button_->SetLabel(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_CLOUD_PRINT_PROXY_DISABLED_BUTTON)));
      enable_disable_button_->InvalidateLayout();
      manage_printer_button_->SetVisible(false);
    }

    // Find the parent ScrollView, and ask it to re-layout since it's
    // possible that the section_description_label_ has changed
    // heights.  And scroll us back to being visible in that case, to
    // be nice to the user.
    views::View* view = section_description_label_->GetParent();
    while (view && view->GetClassName() != views::ScrollView::kViewClassName)
      view = view->GetParent();
    if (view) {
      gfx::Rect visible_bounds = GetVisibleBounds();
      bool was_all_visible = (visible_bounds.size() == bounds().size());
      // Our bounds can change across this call, so we have to use the
      // new bounds if we want to stay completely visible.
      view->Layout();
      ScrollRectToVisible(was_all_visible ? bounds() : visible_bounds);
    } else {
      Layout();
    }
  }
}

bool CloudPrintProxySection::Enabled() const {
  return profile()->GetPrefs()->HasPrefPath(prefs::kCloudPrintEmail) &&
      !profile()->GetPrefs()->GetString(prefs::kCloudPrintEmail).empty();
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView

class AdvancedContentsView : public OptionsPageView {
 public:
  explicit AdvancedContentsView(Profile* profile);
  virtual ~AdvancedContentsView();

  // views::View overrides:
  virtual int GetLineScrollIncrement(views::ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual void Layout();
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

 private:
  static void InitClass();

  static int line_height_;

  DISALLOW_COPY_AND_ASSIGN(AdvancedContentsView);
};

// static
int AdvancedContentsView::line_height_ = 0;

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, public:

AdvancedContentsView::AdvancedContentsView(Profile* profile)
    : OptionsPageView(profile) {
  InitClass();
}

AdvancedContentsView::~AdvancedContentsView() {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, views::View overrides:

int AdvancedContentsView::GetLineScrollIncrement(
    views::ScrollView* scroll_view,
    bool is_horizontal,
    bool is_positive) {

  if (!is_horizontal)
    return line_height_;
  return View::GetPageScrollIncrement(scroll_view, is_horizontal, is_positive);
}

void AdvancedContentsView::Layout() {
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(0, 0, width, height);
  } else {
    gfx::Size prefsize = GetPreferredSize();
    SetBounds(0, 0, prefsize.width(), prefsize.height());
  }
  View::Layout();
}

void AdvancedContentsView::DidChangeBounds(const gfx::Rect& previous,
                                           const gfx::Rect& current) {
  // Override to do nothing. Calling Layout() interferes with our scrolling.
}


////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, OptionsPageView implementation:

void AdvancedContentsView::InitControlLayout() {
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new PrivacySection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new NetworkSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new TranslateSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new DownloadSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new WebContentSection(profile()));
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(new SecuritySection(profile()));
#if defined(GOOGLE_CHROME_BUILD) && defined(OS_WIN)
  // We want to enable the cloud print UI on Windows. Since the cloud
  // print proxy on Windows needs the PDF plugin, we only enable it by
  // default on Google Chrome Windows builds (which contain the PDF
  // plugin).
  bool cloud_print_available = true;
#else
  bool cloud_print_available = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCloudPrintProxy);
#endif
  if (cloud_print_available &&
      profile()->GetCloudPrintProxyService()) {
    layout->StartRow(0, single_column_view_set_id);
    layout->AddView(new CloudPrintProxySection(profile()));
  }
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedContentsView, private:

void AdvancedContentsView::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    line_height_ = rb.GetFont(ResourceBundle::BaseFont).GetHeight();
    initialized = true;
  }
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer, public:

AdvancedScrollViewContainer::AdvancedScrollViewContainer(Profile* profile)
    : contents_view_(new AdvancedContentsView(profile)),
      scroll_view_(new views::ScrollView) {
  AddChildView(scroll_view_);
  scroll_view_->SetContents(contents_view_);
  set_background(new ListBackground());
}

AdvancedScrollViewContainer::~AdvancedScrollViewContainer() {
}

////////////////////////////////////////////////////////////////////////////////
// AdvancedScrollViewContainer, views::View overrides:

void AdvancedScrollViewContainer::Layout() {
  gfx::Rect lb = GetLocalBounds(false);

  gfx::Size border = gfx::NativeTheme::instance()->GetThemeBorderSize(
      gfx::NativeTheme::LIST);
  lb.Inset(border.width(), border.height());
  scroll_view_->SetBounds(lb);
}
