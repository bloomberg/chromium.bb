// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/general_page_view.h"

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/browser/ui/views/keyword_editor_view.h"
#include "chrome/browser/ui/views/options/managed_prefs_banner_view.h"
#include "chrome/browser/ui/views/options/options_group_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/browser_distribution.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

namespace {

// All the options pages are in the same view hierarchy. This means we need to
// make sure group identifiers don't collide across different pages.
const int kStartupRadioGroup = 101;
const int kHomePageRadioGroup = 102;
const SkColor kDefaultBrowserLabelColor = SkColorSetRGB(0, 135, 0);
const SkColor kNotDefaultBrowserLabelColor = SkColorSetRGB(135, 0, 0);
const int kHomePageTextfieldWidthChars = 40;

bool IsNewTabUIURLString(const GURL& url) {
  return url == GURL(chrome::kChromeUINewTabURL);
}
}  // namespace

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupContents
class OptionsGroupContents : public views::View {
 public:
  OptionsGroupContents() { }

  // views::View overrides:
  virtual AccessibilityTypes::Role GetAccessibleRole() {
    return AccessibilityTypes::ROLE_GROUPING;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OptionsGroupContents);
};

///////////////////////////////////////////////////////////////////////////////
// SearchEngineListModel

class SearchEngineListModel : public ui::ComboboxModel,
                              public TemplateURLModelObserver {
 public:
  explicit SearchEngineListModel(Profile* profile);
  virtual ~SearchEngineListModel();

  // Sets the Combobox. SearchEngineListModel needs a handle to the Combobox
  // so that when the TemplateURLModel changes the combobox can be updated.
  void SetCombobox(views::Combobox* combobox);

  // ui::ComboboxModel overrides:
  virtual int GetItemCount();
  virtual string16 GetItemAt(int index);

  // Returns the TemplateURL at the specified index.
  const TemplateURL* GetTemplateURLAt(int index);

  TemplateURLModel* model() { return template_url_model_; }

 private:
  // TemplateURLModelObserver methods.
  virtual void OnTemplateURLModelChanged();

  // Recalculates the TemplateURLs to display and notifies the combobox.
  void ResetContents();

  // Resets the selection of the combobox based on the users selected search
  // engine.
  void ChangeComboboxSelection();

  TemplateURLModel* template_url_model_;

  // The combobox hosting us.
  views::Combobox* combobox_;

  // The TemplateURLs we're showing.
  typedef std::vector<const TemplateURL*> TemplateURLs;
  TemplateURLs template_urls_;

  DISALLOW_COPY_AND_ASSIGN(SearchEngineListModel);
};

SearchEngineListModel::SearchEngineListModel(Profile* profile)
    : template_url_model_(profile->GetTemplateURLModel()),
      combobox_(NULL) {
  if (template_url_model_) {
    template_url_model_->Load();
    template_url_model_->AddObserver(this);
  }
  ResetContents();
}

SearchEngineListModel::~SearchEngineListModel() {
  if (template_url_model_)
    template_url_model_->RemoveObserver(this);
}

void SearchEngineListModel::SetCombobox(views::Combobox* combobox) {
  combobox_ = combobox;
  if (template_url_model_ && template_url_model_->loaded())
    ChangeComboboxSelection();
  else
    combobox_->SetEnabled(false);
}

int SearchEngineListModel::GetItemCount() {
  return static_cast<int>(template_urls_.size());
}

string16 SearchEngineListModel::GetItemAt(int index) {
  DCHECK(index < GetItemCount());
  return WideToUTF16Hack(template_urls_[index]->short_name());
}

const TemplateURL* SearchEngineListModel::GetTemplateURLAt(int index) {
  DCHECK(index >= 0 && index < static_cast<int>(template_urls_.size()));
  return template_urls_[static_cast<int>(index)];
}

void SearchEngineListModel::OnTemplateURLModelChanged() {
  ResetContents();
}

void SearchEngineListModel::ResetContents() {
  if (!template_url_model_ || !template_url_model_->loaded())
    return;
  template_urls_.clear();
  TemplateURLs model_urls = template_url_model_->GetTemplateURLs();
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (model_urls[i]->ShowInDefaultList())
      template_urls_.push_back(model_urls[i]);
  }

  if (combobox_) {
    combobox_->ModelChanged();
    ChangeComboboxSelection();
  }
}

void SearchEngineListModel::ChangeComboboxSelection() {
  if (template_urls_.size()) {
    const TemplateURL* default_search_provider =
        template_url_model_->GetDefaultSearchProvider();
    if (default_search_provider) {
      TemplateURLs::iterator i =
          find(template_urls_.begin(), template_urls_.end(),
               default_search_provider);
      if (i != template_urls_.end()) {
        combobox_->SetSelectedItem(
            static_cast<int>(i - template_urls_.begin()));
      }
    } else {
        combobox_->SetSelectedItem(-1);
    }
  }
  // If the default search is managed or there are no URLs, disable the control.
  combobox_->SetEnabled(!template_urls_.empty() &&
                        !template_url_model_->is_default_search_managed());
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, public:

GeneralPageView::GeneralPageView(Profile* profile)
    : startup_group_(NULL),
      startup_homepage_radio_(NULL),
      startup_last_session_radio_(NULL),
      startup_custom_radio_(NULL),
      startup_add_custom_page_button_(NULL),
      startup_remove_custom_page_button_(NULL),
      startup_use_current_page_button_(NULL),
      startup_custom_pages_table_(NULL),
      homepage_group_(NULL),
      homepage_use_newtab_radio_(NULL),
      homepage_use_url_radio_(NULL),
      homepage_use_url_textfield_(NULL),
      homepage_show_home_button_checkbox_(NULL),
      default_search_group_(NULL),
      default_search_manage_engines_button_(NULL),
      instant_checkbox_(NULL),
      instant_link_(NULL),
      default_browser_group_(NULL),
      default_browser_status_label_(NULL),
      default_browser_use_as_default_button_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          default_browser_worker_(
              new ShellIntegration::DefaultBrowserWorker(this))),
      OptionsPageView(profile) {
}

GeneralPageView::~GeneralPageView() {
  if (startup_custom_pages_table_)
    startup_custom_pages_table_->SetModel(NULL);
  default_browser_worker_->ObserverDestroyed();
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::ButtonListener implementation:

void GeneralPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == startup_homepage_radio_ ||
      sender == startup_last_session_radio_ ||
      sender == startup_custom_radio_) {
    SaveStartupPref();
    if (sender == startup_homepage_radio_) {
      UserMetricsRecordAction(UserMetricsAction("Options_Startup_Homepage"),
                              profile()->GetPrefs());
    } else if (sender == startup_last_session_radio_) {
      UserMetricsRecordAction(UserMetricsAction("Options_Startup_LastSession"),
                              profile()->GetPrefs());
    } else if (sender == startup_custom_radio_) {
      UserMetricsRecordAction(UserMetricsAction("Options_Startup_Custom"),
                              profile()->GetPrefs());
    }
  } else if (sender == startup_add_custom_page_button_) {
    AddURLToStartupURLs();
  } else if (sender == startup_remove_custom_page_button_) {
    RemoveURLsFromStartupURLs();
  } else if (sender == startup_use_current_page_button_) {
    SetStartupURLToCurrentPage();
  } else if (sender == homepage_use_newtab_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Homepage_UseNewTab"),
                            profile()->GetPrefs());
    UpdateHomepagePrefs();
    EnableHomepageURLField(false);
  } else if (sender == homepage_use_url_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Homepage_UseURL"),
                            profile()->GetPrefs());
    UpdateHomepagePrefs();
    EnableHomepageURLField(true);
  } else if (sender == homepage_show_home_button_checkbox_) {
    bool show_button = homepage_show_home_button_checkbox_->checked();
    if (show_button) {
      UserMetricsRecordAction(
                        UserMetricsAction("Options_Homepage_ShowHomeButton"),
                        profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(
                        UserMetricsAction("Options_Homepage_HideHomeButton"),
                        profile()->GetPrefs());
    }
    show_home_button_.SetValue(show_button);
  } else if (sender == default_browser_use_as_default_button_) {
    default_browser_worker_->StartSetAsDefaultBrowser();
    UserMetricsRecordAction(UserMetricsAction("Options_SetAsDefaultBrowser"),
                            NULL);
    // If the user made Chrome the default browser, then he/she arguably wants
    // to be notified when that changes.
    profile()->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, true);
  } else if (sender == default_search_manage_engines_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ManageSearchEngines"),
                            NULL);
    KeywordEditorView::Show(profile());
  } else if (sender == instant_checkbox_) {
    if (instant_checkbox_->checked()) {
      // Don't toggle immediately, instead let
      // ShowInstantConfirmDialogIfNecessary do it.
      instant_checkbox_->SetChecked(false);
      browser::ShowInstantConfirmDialogIfNecessary(
          GetWindow()->GetNativeWindow(), profile());
    } else {
      InstantController::Disable(profile());
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::Combobox::Listener implementation:

void GeneralPageView::ItemChanged(views::Combobox* combobox,
                                  int prev_index, int new_index) {
  if (combobox == default_search_engine_combobox_) {
    SetDefaultSearchProvider();
    UserMetricsRecordAction(UserMetricsAction("Options_SearchEngineChanged"),
                            NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::Textfield::Controller implementation:

void GeneralPageView::ContentsChanged(views::Textfield* sender,
                                      const std::wstring& new_contents) {
  if (sender == homepage_use_url_textfield_) {
    UpdateHomepagePrefs();
  }
}

bool GeneralPageView::HandleKeyEvent(views::Textfield* sender,
                                     const views::KeyEvent& key_event) {
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, OptionsPageView implementation:

void GeneralPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(
      new ManagedPrefsBannerView(profile()->GetPrefs(), OPTIONS_PAGE_GENERAL));

  layout->StartRow(0, single_column_view_set_id);
  InitStartupGroup();
  layout->AddView(startup_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitHomepageGroup();
  layout->AddView(homepage_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  InitDefaultSearchGroup();
  layout->AddView(default_search_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

#if !defined(OS_CHROMEOS)
  layout->StartRow(0, single_column_view_set_id);
  InitDefaultBrowserGroup();
  layout->AddView(default_browser_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
#endif

  // Register pref observers that update the controls when a pref changes.
  registrar_.Init(profile()->GetPrefs());
  registrar_.Add(prefs::kRestoreOnStartup, this);
  registrar_.Add(prefs::kURLsToRestoreOnStartup, this);
  registrar_.Add(prefs::kInstantEnabled, this);

  new_tab_page_is_home_page_.Init(prefs::kHomePageIsNewTabPage,
      profile()->GetPrefs(), this);
  homepage_.Init(prefs::kHomePage, profile()->GetPrefs(), this);
  show_home_button_.Init(prefs::kShowHomeButton, profile()->GetPrefs(), this);
  default_browser_policy_.Init(prefs::kDefaultBrowserSettingEnabled,
                               g_browser_process->local_state(),
                               this);
}

void GeneralPageView::NotifyPrefChanged(const std::string* pref_name) {
  PrefService* prefs = profile()->GetPrefs();
  if (!pref_name ||
      *pref_name == prefs::kRestoreOnStartup ||
      *pref_name == prefs::kURLsToRestoreOnStartup) {
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    bool radio_buttons_enabled = !SessionStartupPref::TypeIsManaged(prefs);
    bool restore_urls_enabled = !SessionStartupPref::URLsAreManaged(prefs);
    switch (startup_pref.type) {
    case SessionStartupPref::DEFAULT:
      startup_homepage_radio_->SetChecked(true);
      restore_urls_enabled = false;
      break;

    case SessionStartupPref::LAST:
      startup_last_session_radio_->SetChecked(true);
      restore_urls_enabled = false;
      break;

    case SessionStartupPref::URLS:
      startup_custom_radio_->SetChecked(true);
      break;
    }
    startup_homepage_radio_->SetEnabled(radio_buttons_enabled);
    startup_last_session_radio_->SetEnabled(radio_buttons_enabled);
    startup_custom_radio_->SetEnabled(radio_buttons_enabled);
    EnableCustomHomepagesControls(restore_urls_enabled);
    startup_custom_pages_table_model_->SetURLs(startup_pref.urls);
  }

  if (!pref_name ||
      *pref_name == prefs::kHomePageIsNewTabPage ||
      *pref_name == prefs::kHomePage) {
    bool new_tab_page_is_home_page_managed =
        new_tab_page_is_home_page_.IsManaged();
    bool homepage_managed = homepage_.IsManaged();
    bool homepage_url_is_new_tab =
        IsNewTabUIURLString(GURL(homepage_.GetValue()));
    bool homepage_is_new_tab = homepage_url_is_new_tab ||
        new_tab_page_is_home_page_.GetValue();
    // If HomepageIsNewTab is managed or
    // Homepage is 'chrome://newtab' and managed, disable the radios.
    bool disable_homepage_choice_buttons =
        new_tab_page_is_home_page_managed ||
        homepage_managed && homepage_url_is_new_tab;
    if (!homepage_url_is_new_tab)
      homepage_use_url_textfield_->SetText(UTF8ToWide(homepage_.GetValue()));
    UpdateHomepageIsNewTabRadio(
        homepage_is_new_tab, !disable_homepage_choice_buttons);
    EnableHomepageURLField(!homepage_is_new_tab);
  }

  if (!pref_name || *pref_name == prefs::kShowHomeButton) {
    homepage_show_home_button_checkbox_->SetChecked(
        show_home_button_.GetValue());
    homepage_show_home_button_checkbox_->SetEnabled(
        !show_home_button_.IsManaged());
  }

  if (!pref_name || *pref_name == prefs::kInstantEnabled)
    instant_checkbox_->SetChecked(prefs->GetBoolean(prefs::kInstantEnabled));

  if (!pref_name || *pref_name == prefs::kDefaultBrowserSettingEnabled) {
    // If the option is managed the UI is uncondionally disabled otherwise we
    // restart the standard button enabling logic.
    if (default_browser_policy_.IsManaged())
      default_browser_use_as_default_button_->SetEnabled(false);
    else
      default_browser_worker_->StartCheckDefaultBrowser();
  }
}

void GeneralPageView::HighlightGroup(OptionsGroup highlight_group) {
  if (highlight_group == OPTIONS_GROUP_DEFAULT_SEARCH)
    default_search_group_->SetHighlighted(true);
}

void GeneralPageView::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == instant_link_);
  browser::ShowOptionsURL(profile(), browser::InstantLearnMoreURL());
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, private:

void GeneralPageView::SetDefaultBrowserUIState(
    ShellIntegration::DefaultBrowserUIState state) {
  bool button_enabled =
      (state == ShellIntegration::STATE_NOT_DEFAULT) &&
      !default_browser_policy_.IsManaged();
  default_browser_use_as_default_button_->SetEnabled(button_enabled);
  default_browser_use_as_default_button_->SetNeedElevation(true);
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    default_browser_status_label_->SetText(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_OPTIONS_DEFAULTBROWSER_DEFAULT,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
    default_browser_status_label_->SetColor(kDefaultBrowserLabelColor);
    Layout();
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    default_browser_status_label_->SetText(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
    default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
    Layout();
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
    default_browser_status_label_->SetText(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
    default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
    Layout();
  }
}

void GeneralPageView::SetDefaultBrowserUIStateForSxS() {
  default_browser_use_as_default_button_->SetEnabled(false);
  default_browser_status_label_->SetText(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_OPTIONS_DEFAULTBROWSER_SXS,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));
  default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
  Layout();
}

void GeneralPageView::InitStartupGroup() {
  startup_homepage_radio_ = new views::RadioButton(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB)),
      kStartupRadioGroup);
  startup_homepage_radio_->set_listener(this);
  startup_last_session_radio_ = new views::RadioButton(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION)),
      kStartupRadioGroup);
  startup_last_session_radio_->set_listener(this);
  startup_last_session_radio_->SetMultiLine(true);
  startup_custom_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_SHOW_PAGES)),
      kStartupRadioGroup);
  startup_custom_radio_->set_listener(this);
  startup_add_custom_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_ADD_BUTTON)));
  startup_remove_custom_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_REMOVE_BUTTON)));
  startup_remove_custom_page_button_->SetEnabled(false);
  startup_use_current_page_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_USE_CURRENT)));

  startup_custom_pages_table_model_.reset(
      new CustomHomePagesTableModel(profile()));
  std::vector<TableColumn> columns;
  columns.push_back(TableColumn());
  startup_custom_pages_table_ = new views::TableView(
      startup_custom_pages_table_model_.get(), columns,
      views::ICON_AND_TEXT, false, false, true);
  startup_custom_pages_table_->SetObserver(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new OptionsGroupContents;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_homepage_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_last_session_radio_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_custom_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(startup_custom_pages_table_, 1, 1,
                  GridLayout::FILL, GridLayout::FILL);

  views::View* button_stack = new views::View;
  GridLayout* button_stack_layout = new GridLayout(button_stack);
  button_stack->SetLayoutManager(button_stack_layout);

  column_set = button_stack_layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_add_custom_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_remove_custom_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  button_stack_layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  button_stack_layout->StartRow(0, single_column_view_set_id);
  button_stack_layout->AddView(startup_use_current_page_button_,
                               1, 1, GridLayout::FILL, GridLayout::CENTER);
  layout->AddView(button_stack);

  startup_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_GROUP_NAME)),
      std::wstring(), true);
}

void GeneralPageView::InitHomepageGroup() {
  homepage_use_newtab_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB)),
      kHomePageRadioGroup);
  homepage_use_newtab_radio_->set_listener(this);
  homepage_use_newtab_radio_->SetMultiLine(true);
  homepage_use_url_radio_ = new views::RadioButton(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_HOMEPAGE_USE_URL)),
      kHomePageRadioGroup);
  homepage_use_url_radio_->set_listener(this);
  homepage_use_url_textfield_ = new views::Textfield;
  homepage_use_url_textfield_->SetController(this);
  homepage_use_url_textfield_->set_default_width_in_chars(
      kHomePageTextfieldWidthChars);
  homepage_show_home_button_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON)));
  homepage_show_home_button_checkbox_->set_listener(this);
  homepage_show_home_button_checkbox_->SetMultiLine(true);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(homepage_use_newtab_radio_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(homepage_use_url_radio_);
  layout->AddView(homepage_use_url_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(homepage_show_home_button_checkbox_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);

  homepage_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_OPTIONS_HOMEPAGE_GROUP_NAME)),
      std::wstring(), true);
}


void GeneralPageView::InitDefaultSearchGroup() {
  default_search_engines_model_.reset(new SearchEngineListModel(profile()));
  default_search_engine_combobox_ =
      new views::Combobox(default_search_engines_model_.get());
  default_search_engines_model_->SetCombobox(default_search_engine_combobox_);
  default_search_engine_combobox_->set_listener(this);

  default_search_manage_engines_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK)));

  instant_checkbox_ = new views::Checkbox(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_INSTANT_PREF)));
  instant_checkbox_->SetMultiLine(false);
  instant_checkbox_->set_listener(this);

  instant_link_ = new views::Link(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
  instant_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  instant_link_->SetController(this);

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int double_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  const int checkbox_column_view_set_id = 1;
  column_set = layout->AddColumnSet(checkbox_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  const int link_column_set_id = 2;
  column_set = layout->AddColumnSet(link_column_set_id);
  // TODO(sky): this isn't right, we need a method to determine real indent.
  column_set->AddPaddingColumn(0, views::Checkbox::GetTextIndent() + 3);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(default_search_engine_combobox_);
  layout->AddView(default_search_manage_engines_button_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, checkbox_column_view_set_id);
  layout->AddView(instant_checkbox_);
  layout->AddPaddingRow(0, 0);

  layout->StartRow(0, link_column_set_id);
  layout->AddView(new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_INSTANT_PREF_WARNING))));
  layout->AddView(instant_link_);

  default_search_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME)),
      std::wstring(), true);
}

void GeneralPageView::InitDefaultBrowserGroup() {
  default_browser_status_label_ = new views::Label;
  default_browser_status_label_->SetMultiLine(true);
  default_browser_status_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  default_browser_use_as_default_button_ = new views::NativeButton(
      this,
      UTF16ToWide(
          l10n_util::GetStringFUTF16(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME))));

  using views::GridLayout;
  using views::ColumnSet;

  views::View* contents = new views::View;
  GridLayout* layout = new GridLayout(contents);
  contents->SetLayoutManager(layout);

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(default_browser_status_label_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(default_browser_use_as_default_button_);

  default_browser_group_ = new OptionsGroupView(
      contents,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME)),
      std::wstring(), false);

  if (BrowserDistribution::GetDistribution()->CanSetAsDefault())
    default_browser_worker_->StartCheckDefaultBrowser();
  else
    SetDefaultBrowserUIStateForSxS();
}

void GeneralPageView::SaveStartupPref() {
  SessionStartupPref pref;

  if (startup_last_session_radio_->checked()) {
    pref.type = SessionStartupPref::LAST;
  } else if (startup_custom_radio_->checked()) {
    pref.type = SessionStartupPref::URLS;
  }

  pref.urls = startup_custom_pages_table_model_->GetURLs();

  SessionStartupPref::SetStartupPref(profile()->GetPrefs(), pref);
}

void GeneralPageView::AddURLToStartupURLs() {
  UrlPicker* dialog = new UrlPicker(this, profile());
  dialog->Show(GetWindow()->GetNativeWindow());
}

void GeneralPageView::RemoveURLsFromStartupURLs() {
  int selected_row = 0;
  for (views::TableView::iterator i =
       startup_custom_pages_table_->SelectionBegin();
       i != startup_custom_pages_table_->SelectionEnd(); ++i) {
    startup_custom_pages_table_model_->Remove(*i);
    selected_row = *i;
  }
  int row_count = startup_custom_pages_table_->RowCount();
  if (selected_row >= row_count)
    selected_row = row_count - 1;
  if (selected_row >= 0) {
    // Select the next row after the last row deleted, or the above item if the
    // latest item was deleted or nothing when the table doesn't have any items.
    startup_custom_pages_table_->Select(selected_row);
  }
  SaveStartupPref();
}

void GeneralPageView::SetStartupURLToCurrentPage() {
  startup_custom_pages_table_model_->SetToCurrentlyOpenPages();

  SaveStartupPref();
}

void GeneralPageView::EnableCustomHomepagesControls(bool enable) {
  startup_add_custom_page_button_->SetEnabled(enable);
  bool has_selected_rows = startup_custom_pages_table_->SelectedRowCount() > 0;
  startup_remove_custom_page_button_->SetEnabled(enable && has_selected_rows);
  startup_use_current_page_button_->SetEnabled(enable);
  startup_custom_pages_table_->SetEnabled(enable);
}

void GeneralPageView::AddBookmark(UrlPicker* dialog,
                                  const std::wstring& title,
                                  const GURL& url) {
  // The restore URLs policy might have become managed while the dialog is
  // displayed.  While the model makes sure that no changes are made in this
  // condition, we should still avoid changing the graphic elements.
  if (SessionStartupPref::URLsAreManaged(profile()->GetPrefs()))
    return;
  int index = startup_custom_pages_table_->FirstSelectedRow();
  if (index == -1)
    index = startup_custom_pages_table_model_->RowCount();
  else
    index++;
  startup_custom_pages_table_model_->Add(index, url);
  startup_custom_pages_table_->Select(index);

  SaveStartupPref();
}

void GeneralPageView::UpdateHomepagePrefs() {
  // If the text field contains a valid URL, sync it to prefs. We run it
  // through the fixer upper to allow input like "google.com" to be converted
  // to something valid ("http://google.com"). If the field contains an
  // empty or null-host URL, a blank homepage is synced to prefs.
  const GURL& homepage =
    URLFixerUpper::FixupURL(
       UTF16ToUTF8(homepage_use_url_textfield_->text()), std::string());
  bool new_tab_page_is_home_page = homepage_use_newtab_radio_->checked();
  if (IsNewTabUIURLString(homepage)) {  // 'chrome://newtab/'
    // This should be handled differently than invalid URLs.
    // When the control arrives here, then |homepage| contains
    // 'chrome://newtab/', and the homepage preference contains the previous
    // valid content of the textfield (fixed up), most likely
    // 'chrome://newta/'. This has to be cleared, because keeping it makes no
    // sense to the user.
    new_tab_page_is_home_page = true;
    homepage_.SetValueIfNotManaged(std::string());
  } else if (!homepage.is_valid()) {
    new_tab_page_is_home_page = true;
    // The URL is invalid either with a host (e.g. http://chr%mium.org)
    // or without a host (e.g. http://). In case there is a host, then
    // the URL is not cleared, saving a fragment of the URL to the
    // preferences (e.g. http://chr in case the characters of the above example
    // were typed by the user one by one).
    // See bug 40996.
    if (!homepage.has_host())
      homepage_.SetValueIfNotManaged(std::string());
  } else {
    homepage_.SetValueIfNotManaged(homepage.spec());
  }
  new_tab_page_is_home_page_.SetValueIfNotManaged(new_tab_page_is_home_page);
}

void GeneralPageView::UpdateHomepageIsNewTabRadio(bool homepage_is_new_tab,
                                                  bool enabled) {
  homepage_use_newtab_radio_->SetChecked(homepage_is_new_tab);
  homepage_use_url_radio_->SetChecked(!homepage_is_new_tab);
  homepage_use_newtab_radio_->SetEnabled(enabled);
  homepage_use_url_radio_->SetEnabled(enabled);
}

void GeneralPageView::OnSelectionChanged() {
  startup_remove_custom_page_button_->SetEnabled(
      startup_custom_pages_table_->SelectedRowCount() > 0);
}

void GeneralPageView::EnableHomepageURLField(bool enabled) {
  if (homepage_.IsManaged()) {
    enabled = false;
  }
  homepage_use_url_textfield_->SetEnabled(enabled);
  homepage_use_url_textfield_->SetReadOnly(!enabled);
}

void GeneralPageView::SetDefaultSearchProvider() {
  const int index = default_search_engine_combobox_->selected_item();
  default_search_engines_model_->model()->SetDefaultSearchProvider(
      default_search_engines_model_->GetTemplateURLAt(index));
}
