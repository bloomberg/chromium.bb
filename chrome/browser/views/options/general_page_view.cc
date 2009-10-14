// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/general_page_view.h"

#include "app/combobox_model.h"
#include "app/gfx/codec/png_codec.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_ui/new_tab_ui.h"
#include "chrome/browser/favicon_service.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/views/keyword_editor_view.h"
#include "chrome/browser/views/options/options_group_view.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

namespace {

static const int kStartupRadioGroup = 1;
static const int kHomePageRadioGroup = 2;
static const SkColor kDefaultBrowserLabelColor = SkColorSetRGB(0, 135, 0);
static const SkColor kNotDefaultBrowserLabelColor = SkColorSetRGB(135, 0, 0);

std::wstring GetNewTabUIURLString() {
  return UTF8ToWide(chrome::kChromeUINewTabURL);
}
}

///////////////////////////////////////////////////////////////////////////////
// CustomHomePagesTableModel

// CustomHomePagesTableModel is the model for the TableView showing the list
// of pages the user wants opened on startup.

class CustomHomePagesTableModel : public TableModel {
 public:
  explicit CustomHomePagesTableModel(Profile* profile);
  virtual ~CustomHomePagesTableModel() {}

  // Sets the set of urls that this model contains.
  void SetURLs(const std::vector<GURL>& urls);

  // Adds an entry at the specified index.
  void Add(int index, const GURL& url);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Returns the set of urls this model contains.
  std::vector<GURL> GetURLs();

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual SkBitmap GetIcon(int row);
  virtual void SetObserver(TableModelObserver* observer);

 private:
  // Each item in the model is represented as an Entry. Entry stores the URL
  // and favicon of the page.
  struct Entry {
    Entry() : fav_icon_handle(0) {}

    // URL of the page.
    GURL url;

    // Icon for the page.
    SkBitmap icon;

    // If non-zero, indicates we're loading the favicon for the page.
    FaviconService::Handle fav_icon_handle;
  };

  static void InitClass();

  // Loads the favicon for the specified entry.
  void LoadFavIcon(Entry* entry);

  // Callback from history service. Updates the icon of the Entry whose
  // fav_icon_handle matches handle and notifies the observer of the change.
  void OnGotFavIcon(FaviconService::Handle handle,
                    bool know_fav_icon,
                    scoped_refptr<RefCountedBytes> image_data,
                    bool is_expired,
                    GURL icon_url);

  // Returns the entry whose fav_icon_handle matches handle and sets entry_index
  // to the index of the entry.
  Entry* GetEntryByLoadHandle(FaviconService::Handle handle, int* entry_index);

  // Set of entries we're showing.
  std::vector<Entry> entries_;

  // Default icon to show when one can't be found for the URL.
  static SkBitmap default_favicon_;

  // Profile used to load icons.
  Profile* profile_;

  TableModelObserver* observer_;

  // Used in loading favicons.
  CancelableRequestConsumer fav_icon_consumer_;

  DISALLOW_COPY_AND_ASSIGN(CustomHomePagesTableModel);
};

// static
SkBitmap CustomHomePagesTableModel::default_favicon_;

CustomHomePagesTableModel::CustomHomePagesTableModel(Profile* profile)
    : profile_(profile),
      observer_(NULL) {
  InitClass();
}

void CustomHomePagesTableModel::SetURLs(const std::vector<GURL>& urls) {
  entries_.resize(urls.size());
  for (size_t i = 0; i < urls.size(); ++i) {
    entries_[i].url = urls[i];
    LoadFavIcon(&(entries_[i]));
  }
  // Complete change, so tell the view to just rebuild itself.
  if (observer_)
    observer_->OnModelChanged();
}

void CustomHomePagesTableModel::Add(int index, const GURL& url) {
  DCHECK(index >= 0 && index <= RowCount());
  entries_.insert(entries_.begin() + static_cast<size_t>(index), Entry());
  entries_[index].url = url;
  LoadFavIcon(&(entries_[index]));
  if (observer_)
    observer_->OnItemsAdded(index, 1);
}

void CustomHomePagesTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  Entry* entry = &(entries_[index]);
  if (entry->fav_icon_handle) {
    // Pending load request, cancel it now so we don't deref a bogus pointer
    // when we get loaded notification.
    FaviconService* favicon_service =
        profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
    if (favicon_service)
      favicon_service->CancelRequest(entry->fav_icon_handle);
  }
  entries_.erase(entries_.begin() + static_cast<size_t>(index));
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

std::vector<GURL> CustomHomePagesTableModel::GetURLs() {
  std::vector<GURL> urls(entries_.size());
  for (size_t i = 0; i < entries_.size(); ++i)
    urls[i] = entries_[i].url;
  return urls;
}

int CustomHomePagesTableModel::RowCount() {
  return static_cast<int>(entries_.size());
}

std::wstring CustomHomePagesTableModel::GetText(int row, int column_id) {
  DCHECK(column_id == 0);
  DCHECK(row >= 0 && row < RowCount());
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  // No need to force URL to have LTR directionality because the custom home
  // pages control is created using LTR directionality.
  return net::FormatUrl(entries_[row].url, languages);
}

SkBitmap CustomHomePagesTableModel::GetIcon(int row) {
  DCHECK(row >= 0 && row < RowCount());
  if (!entries_[row].icon.isNull())
    return entries_[row].icon;
  return default_favicon_;
}

void CustomHomePagesTableModel::SetObserver(TableModelObserver* observer) {
  observer_ = observer;
}

void CustomHomePagesTableModel::InitClass() {
  static bool initialized = false;
  if (!initialized) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_favicon_ = *rb.GetBitmapNamed(IDR_DEFAULT_FAVICON);
    initialized = true;
  }
}

void CustomHomePagesTableModel::LoadFavIcon(Entry* entry) {
  FaviconService* favicon_service =
      profile_->GetFaviconService(Profile::EXPLICIT_ACCESS);
  if (!favicon_service)
    return;
  entry->fav_icon_handle = favicon_service->GetFaviconForURL(
      entry->url, &fav_icon_consumer_,
      NewCallback(this, &CustomHomePagesTableModel::OnGotFavIcon));
}

void CustomHomePagesTableModel::OnGotFavIcon(
    FaviconService::Handle handle,
    bool know_fav_icon,
    scoped_refptr<RefCountedBytes> image_data,
    bool is_expired,
    GURL icon_url) {
  int entry_index;
  Entry* entry = GetEntryByLoadHandle(handle, &entry_index);
  DCHECK(entry);
  entry->fav_icon_handle = 0;
  if (know_fav_icon && image_data.get() && !image_data->data.empty()) {
    int width, height;
    std::vector<unsigned char> decoded_data;
    if (gfx::PNGCodec::Decode(&image_data->data.front(),
                              image_data->data.size(),
                              gfx::PNGCodec::FORMAT_BGRA, &decoded_data,
                              &width, &height)) {
      entry->icon.setConfig(SkBitmap::kARGB_8888_Config, width, height);
      entry->icon.allocPixels();
      memcpy(entry->icon.getPixels(), &decoded_data.front(),
             width * height * 4);
      if (observer_)
        observer_->OnItemsChanged(static_cast<int>(entry_index), 1);
    }
  }
}

CustomHomePagesTableModel::Entry*
    CustomHomePagesTableModel::GetEntryByLoadHandle(
    FaviconService::Handle handle,
    int* index) {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].fav_icon_handle == handle) {
      *index = static_cast<int>(i);
      return &entries_[i];
    }
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// SearchEngineListModel

class SearchEngineListModel : public ComboboxModel,
                              public TemplateURLModelObserver {
 public:
  explicit SearchEngineListModel(Profile* profile);
  virtual ~SearchEngineListModel();

  // Sets the Combobox. SearchEngineListModel needs a handle to the Combobox
  // so that when the TemplateURLModel changes the combobox can be updated.
  void SetCombobox(views::Combobox* combobox);

  // ComboboxModel overrides:
  virtual int GetItemCount();
  virtual std::wstring GetItemAt(int index);

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

std::wstring SearchEngineListModel::GetItemAt(int index) {
  DCHECK(index < GetItemCount());
  return template_urls_[index]->short_name();
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
    combobox_->SetEnabled(true);

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
    }
  } else {
    combobox_->SetEnabled(false);
  }
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
      default_browser_group_(NULL),
      default_browser_status_label_(NULL),
      default_browser_use_as_default_button_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          default_browser_worker_(
              new ShellIntegration::DefaultBrowserWorker(this))),
      OptionsPageView(profile) {
}

GeneralPageView::~GeneralPageView() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->RemovePrefObserver(
      prefs::kURLsToRestoreOnStartup, this);
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
      UserMetricsRecordAction(L"Options_Startup_Homepage",
                              profile()->GetPrefs());
    } else if (sender == startup_last_session_radio_) {
      UserMetricsRecordAction(L"Options_Startup_LastSession",
                              profile()->GetPrefs());
    } else if (sender == startup_custom_radio_) {
      UserMetricsRecordAction(L"Options_Startup_Custom",
                              profile()->GetPrefs());
    }
  } else if (sender == startup_add_custom_page_button_) {
    AddURLToStartupURLs();
  } else if (sender == startup_remove_custom_page_button_) {
    RemoveURLsFromStartupURLs();
  } else if (sender == startup_use_current_page_button_) {
    SetStartupURLToCurrentPage();
  } else if (sender == homepage_use_newtab_radio_) {
    UserMetricsRecordAction(L"Options_Homepage_UseNewTab",
                            profile()->GetPrefs());
    SetHomepage(GetNewTabUIURLString());
    EnableHomepageURLField(false);
  } else if (sender == homepage_use_url_radio_) {
    UserMetricsRecordAction(L"Options_Homepage_UseURL",
                            profile()->GetPrefs());
    SetHomepage(homepage_use_url_textfield_->text());
    EnableHomepageURLField(true);
  } else if (sender == homepage_show_home_button_checkbox_) {
    bool show_button = homepage_show_home_button_checkbox_->checked();
    if (show_button) {
      UserMetricsRecordAction(L"Options_Homepage_ShowHomeButton",
                              profile()->GetPrefs());
    } else {
      UserMetricsRecordAction(L"Options_Homepage_HideHomeButton",
                              profile()->GetPrefs());
    }
    show_home_button_.SetValue(show_button);
  } else if (sender == default_browser_use_as_default_button_) {
    default_browser_worker_->StartSetAsDefaultBrowser();
    UserMetricsRecordAction(L"Options_SetAsDefaultBrowser", NULL);
    // If the user made Chrome the default browser, then he/she arguably wants
    // to be notified when that changes.
    profile()->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, true);
  } else if (sender == default_search_manage_engines_button_) {
    UserMetricsRecordAction(L"Options_ManageSearchEngines", NULL);
    KeywordEditorView::Show(profile());
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::Combobox::Listener implementation:

void GeneralPageView::ItemChanged(views::Combobox* combobox,
                                  int prev_index, int new_index) {
  if (combobox == default_search_engine_combobox_) {
    SetDefaultSearchProvider();
    UserMetricsRecordAction(L"Options_SearchEngineChanged", NULL);
  }
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::Textfield::Controller implementation:

void GeneralPageView::ContentsChanged(views::Textfield* sender,
                                      const std::wstring& new_contents) {
  if (sender == homepage_use_url_textfield_) {
    // If the text field contains a valid URL, sync it to prefs. We run it
    // through the fixer upper to allow input like "google.com" to be converted
    // to something valid ("http://google.com").
    std::wstring url_string = URLFixerUpper::FixupURL(
        homepage_use_url_textfield_->text(), std::wstring());
    if (GURL(url_string).is_valid())
      SetHomepage(url_string);
  }
}

bool GeneralPageView::HandleKeystroke(views::Textfield* sender,
                                      const views::Textfield::Keystroke&) {
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

  layout->StartRow(0, single_column_view_set_id);
  InitDefaultBrowserGroup();
  layout->AddView(default_browser_group_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Register pref observers that update the controls when a pref changes.
  profile()->GetPrefs()->AddPrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->AddPrefObserver(prefs::kURLsToRestoreOnStartup, this);

  new_tab_page_is_home_page_.Init(prefs::kHomePageIsNewTabPage,
      profile()->GetPrefs(), this);
  homepage_.Init(prefs::kHomePage, profile()->GetPrefs(), this);
  show_home_button_.Init(prefs::kShowHomeButton, profile()->GetPrefs(), this);
}

void GeneralPageView::NotifyPrefChanged(const std::wstring* pref_name) {
  if (!pref_name || *pref_name == prefs::kRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    switch (startup_pref.type) {
    case SessionStartupPref::DEFAULT:
      startup_homepage_radio_->SetChecked(true);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::LAST:
      startup_last_session_radio_->SetChecked(true);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::URLS:
      startup_custom_radio_->SetChecked(true);
      EnableCustomHomepagesControls(true);
      break;
    }
  }

  // TODO(beng): Note that the kURLsToRestoreOnStartup pref is a mutable list,
  //             and changes to mutable lists aren't broadcast through the
  //             observer system, so the second half of this condition will
  //             never match. Once support for broadcasting such updates is
  //             added, this will automagically start to work, and this comment
  //             can be removed.
  if (!pref_name || *pref_name == prefs::kURLsToRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    startup_custom_pages_table_model_->SetURLs(startup_pref.urls);
  }

  if (!pref_name || *pref_name == prefs::kHomePageIsNewTabPage) {
    if (new_tab_page_is_home_page_.GetValue()) {
      homepage_use_newtab_radio_->SetChecked(true);
      EnableHomepageURLField(false);
    } else {
      homepage_use_url_radio_->SetChecked(true);
      EnableHomepageURLField(true);
    }
  }

  if (!pref_name || *pref_name == prefs::kHomePage) {
    bool enabled = homepage_.GetValue() != GetNewTabUIURLString();
    if (enabled)
      homepage_use_url_textfield_->SetText(homepage_.GetValue());
  }

  if (!pref_name || *pref_name == prefs::kShowHomeButton) {
    homepage_show_home_button_checkbox_->SetChecked(
        show_home_button_.GetValue());
  }
}

void GeneralPageView::HighlightGroup(OptionsGroup highlight_group) {
  if (highlight_group == OPTIONS_GROUP_DEFAULT_SEARCH)
    default_search_group_->SetHighlighted(true);
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, views::View overrides:

void GeneralPageView::Layout() {
  // We need to Layout twice - once to get the width of the contents box...
  View::Layout();
  startup_last_session_radio_->SetBounds(
      0, 0, startup_group_->GetContentsWidth(), 0);
  homepage_use_newtab_radio_->SetBounds(
      0, 0, homepage_group_->GetContentsWidth(), 0);
  homepage_show_home_button_checkbox_->SetBounds(
      0, 0, homepage_group_->GetContentsWidth(), 0);
  default_browser_status_label_->SetBounds(
      0, 0, default_browser_group_->GetContentsWidth(), 0);
  // ... and twice to get the height of multi-line items correct.
  View::Layout();
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageView, private:

void GeneralPageView::SetDefaultBrowserUIState(
    ShellIntegration::DefaultBrowserUIState state) {
  bool button_enabled = state == ShellIntegration::STATE_NOT_DEFAULT;
  default_browser_use_as_default_button_->SetEnabled(button_enabled);
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    default_browser_status_label_->SetText(
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_DEFAULT,
                            l10n_util::GetString(IDS_PRODUCT_NAME)));
    default_browser_status_label_->SetColor(kDefaultBrowserLabelColor);
    Layout();
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    default_browser_status_label_->SetText(
        l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
                              l10n_util::GetString(IDS_PRODUCT_NAME)));
    default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
    Layout();
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
    default_browser_status_label_->SetText(
        l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
                              l10n_util::GetString(IDS_PRODUCT_NAME)));
    default_browser_status_label_->SetColor(kNotDefaultBrowserLabelColor);
    Layout();
  }
}

void GeneralPageView::InitStartupGroup() {
  startup_homepage_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB),
      kStartupRadioGroup);
  startup_homepage_radio_->set_listener(this);
  startup_last_session_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION),
      kStartupRadioGroup);
  startup_last_session_radio_->set_listener(this);
  startup_last_session_radio_->SetMultiLine(true);
  startup_custom_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_PAGES),
      kStartupRadioGroup);
  startup_custom_radio_->set_listener(this);
  startup_add_custom_page_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_STARTUP_ADD_BUTTON));
  startup_remove_custom_page_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_STARTUP_REMOVE_BUTTON));
  startup_remove_custom_page_button_->SetEnabled(false);
  startup_use_current_page_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_STARTUP_USE_CURRENT));

  startup_custom_pages_table_model_.reset(
      new CustomHomePagesTableModel(profile()));
  std::vector<TableColumn> columns;
  columns.push_back(TableColumn());
  startup_custom_pages_table_ = new views::TableView(
      startup_custom_pages_table_model_.get(), columns,
      views::ICON_AND_TEXT, false, false, true);
  // URLs are inherently left-to-right, so do not mirror the table.
  startup_custom_pages_table_->EnableUIMirroringForRTLLanguages(false);
  startup_custom_pages_table_->SetObserver(this);

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
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 0,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_homepage_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_last_session_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(startup_custom_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(startup_custom_pages_table_);

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
      contents, l10n_util::GetString(IDS_OPTIONS_STARTUP_GROUP_NAME),
      EmptyWString(), true);
}

void GeneralPageView::InitHomepageGroup() {
  homepage_use_newtab_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB),
      kHomePageRadioGroup);
  homepage_use_newtab_radio_->set_listener(this);
  homepage_use_newtab_radio_->SetMultiLine(true);
  homepage_use_url_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_URL),
      kHomePageRadioGroup);
  homepage_use_url_radio_->set_listener(this);
  homepage_use_url_textfield_ = new views::Textfield;
  homepage_use_url_textfield_->SetController(this);
  homepage_show_home_button_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON));
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
  layout->AddView(homepage_use_newtab_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(homepage_use_url_radio_);
  layout->AddView(homepage_use_url_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(homepage_show_home_button_checkbox_);

  homepage_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_GROUP_NAME),
      EmptyWString(), true);
}


void GeneralPageView::InitDefaultSearchGroup() {
  default_search_engines_model_.reset(new SearchEngineListModel(profile()));
  default_search_engine_combobox_ =
      new views::Combobox(default_search_engines_model_.get());
  default_search_engines_model_->SetCombobox(default_search_engine_combobox_);
  default_search_engine_combobox_->set_listener(this);

  default_search_manage_engines_button_ = new views::NativeButton(
      this,
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK));

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

  layout->StartRow(0, double_column_view_set_id);
  layout->AddView(default_search_engine_combobox_);
  layout->AddView(default_search_manage_engines_button_);

  default_search_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME),
      EmptyWString(), true);
}

void GeneralPageView::InitDefaultBrowserGroup() {
  default_browser_status_label_ = new views::Label;
  default_browser_status_label_->SetMultiLine(true);
  default_browser_status_label_->SetHorizontalAlignment(
      views::Label::ALIGN_LEFT);
  default_browser_use_as_default_button_ = new views::NativeButton(
      this,
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
                            l10n_util::GetString(IDS_PRODUCT_NAME)));

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
  layout->AddView(default_browser_status_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_view_set_id);
  layout->AddView(default_browser_use_as_default_button_);

  default_browser_group_ = new OptionsGroupView(
      contents, l10n_util::GetString(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME),
      EmptyWString(), false);

  default_browser_worker_->StartCheckDefaultBrowser();
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
  UrlPicker* dialog = new UrlPicker(this, profile(), false);
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
  // Remove the current entries.
  while (startup_custom_pages_table_model_->RowCount())
    startup_custom_pages_table_model_->Remove(0);

  // And add all entries for all open browsers with our profile.
  int add_index = 0;
  for (BrowserList::const_iterator browser_i = BrowserList::begin();
       browser_i != BrowserList::end(); ++browser_i) {
    Browser* browser = *browser_i;
    if (browser->profile() != profile())
      continue;  // Only want entries for open profile.

    for (int tab_index = 0; tab_index < browser->tab_count(); ++tab_index) {
      TabContents* tab = browser->GetTabContentsAt(tab_index);
      if (tab->ShouldDisplayURL()) {
        const GURL url = browser->GetTabContentsAt(tab_index)->GetURL();
        if (!url.is_empty())
          startup_custom_pages_table_model_->Add(add_index++, url);
      }
    }
  }

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
  int index = startup_custom_pages_table_->FirstSelectedRow();
  if (index == -1)
    index = startup_custom_pages_table_model_->RowCount();
  else
    index++;
  startup_custom_pages_table_model_->Add(index, url);
  startup_custom_pages_table_->Select(index);

  SaveStartupPref();
}

void GeneralPageView::SetHomepage(const std::wstring& homepage) {
  if (homepage.empty() || homepage == GetNewTabUIURLString()) {
    new_tab_page_is_home_page_.SetValue(true);
  } else {
    new_tab_page_is_home_page_.SetValue(false);
    homepage_.SetValue(homepage);
  }
}

void GeneralPageView::OnSelectionChanged() {
  startup_remove_custom_page_button_->SetEnabled(
      startup_custom_pages_table_->SelectedRowCount() > 0);
}

void GeneralPageView::EnableHomepageURLField(bool enabled) {
  if (enabled) {
    homepage_use_url_textfield_->SetEnabled(true);
    homepage_use_url_textfield_->SetReadOnly(false);
  } else {
    homepage_use_url_textfield_->SetEnabled(false);
    homepage_use_url_textfield_->SetReadOnly(true);
  }
}

void GeneralPageView::SetDefaultSearchProvider() {
  const int index = default_search_engine_combobox_->selected_item();
  default_search_engines_model_->model()->SetDefaultSearchProvider(
      default_search_engines_model_->GetTemplateURLAt(index));
}
