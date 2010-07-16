// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/general_page_gtk.h"

#include <set>
#include <vector>

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/gtk_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/custom_home_pages_table_model.h"
#include "chrome/browser/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/keyword_editor_view.h"
#include "chrome/browser/gtk/options/managed_prefs_banner_gtk.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/browser/gtk/options/url_picker_dialog_gtk.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/session_startup_pref.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

namespace {

// Markup for the text showing the current state of the default browser
const char kDefaultBrowserLabelMarkup[] = "<span color='#%s'>%s</span>";

// Color of the default browser text when Chromium is the default browser
const char kDefaultBrowserLabelColor[] = "008700";

// Color of the default browser text when Chromium is not the default browser
const char kNotDefaultBrowserLabelColor[] = "870000";

// Column ids for |startup_custom_pages_store_|.
enum {
  COL_FAVICON,
  COL_URL,
  COL_TOOLTIP,
  COL_COUNT,
};

// Column ids for |default_search_engines_model_|.
enum {
  SEARCH_ENGINES_COL_INDEX,
  SEARCH_ENGINES_COL_TITLE,
  SEARCH_ENGINES_COL_COUNT,
};

bool IsNewTabUIURLString(const GURL& url) {
  return url == GURL(chrome::kChromeUINewTabURL);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, public:

GeneralPageGtk::GeneralPageGtk(Profile* profile)
    : OptionsPageBase(profile),
      template_url_model_(NULL),
      default_search_initializing_(true),
      initializing_(true),
      default_browser_worker_(
          new ShellIntegration::DefaultBrowserWorker(this)),
      managed_prefs_banner_(profile->GetPrefs(), OPTIONS_PAGE_GENERAL) {
  scoped_ptr<OptionsLayoutBuilderGtk>
    options_builder(OptionsLayoutBuilderGtk::CreateOptionallyCompactLayout());
  page_ = options_builder->get_page_widget();
  accessible_widget_helper_.reset(new AccessibleWidgetHelper(page_, profile));

  options_builder->AddWidget(managed_prefs_banner_.banner_widget(), false);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_GROUP_NAME),
      InitStartupGroup(), true);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_GROUP_NAME),
      InitHomepageGroup(), false);
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME),
      InitDefaultSearchGroup(), false);
#if !defined(OS_CHROMEOS)
  options_builder->AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME),
      InitDefaultBrowserGroup(), false);
#endif

  profile->GetPrefs()->AddPrefObserver(prefs::kRestoreOnStartup, this);
  profile->GetPrefs()->AddPrefObserver(prefs::kURLsToRestoreOnStartup, this);

  new_tab_page_is_home_page_.Init(prefs::kHomePageIsNewTabPage,
      profile->GetPrefs(), this);
  homepage_.Init(prefs::kHomePage, profile->GetPrefs(), this);
  show_home_button_.Init(prefs::kShowHomeButton, profile->GetPrefs(), this);

  // Load initial values
  NotifyPrefChanged(NULL);
}

GeneralPageGtk::~GeneralPageGtk() {
  profile()->GetPrefs()->RemovePrefObserver(prefs::kRestoreOnStartup, this);
  profile()->GetPrefs()->RemovePrefObserver(
      prefs::kURLsToRestoreOnStartup, this);

  if (template_url_model_)
    template_url_model_->RemoveObserver(this);

  default_browser_worker_->ObserverDestroyed();
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, OptionsPageBase overrides:

void GeneralPageGtk::NotifyPrefChanged(const std::wstring* pref_name) {
  initializing_ = true;
  if (!pref_name || *pref_name == prefs::kRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
    switch (startup_pref.type) {
    case SessionStartupPref::DEFAULT:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(startup_homepage_radio_),
                                   TRUE);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::LAST:
      gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(startup_last_session_radio_), TRUE);
      EnableCustomHomepagesControls(false);
      break;

    case SessionStartupPref::URLS:
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(startup_custom_radio_),
                                   TRUE);
      EnableCustomHomepagesControls(true);
      break;
    }
  }

  if (!pref_name || *pref_name == prefs::kURLsToRestoreOnStartup) {
    PrefService* prefs = profile()->GetPrefs();
    const SessionStartupPref startup_pref =
        SessionStartupPref::GetStartupPref(prefs);
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
        (homepage_managed && homepage_url_is_new_tab);
    if (!homepage_url_is_new_tab) {
      gtk_entry_set_text(GTK_ENTRY(homepage_use_url_entry_),
                         homepage_.GetValue().c_str());
    }
    UpdateHomepageIsNewTabRadio(
        homepage_is_new_tab, !disable_homepage_choice_buttons);
    EnableHomepageURLField(!homepage_is_new_tab);
  }

  if (!pref_name || *pref_name == prefs::kShowHomeButton) {
    gtk_toggle_button_set_active(
        GTK_TOGGLE_BUTTON(homepage_show_home_button_checkbox_),
        show_home_button_.GetValue());
  }

  initializing_ = false;
}

void GeneralPageGtk::HighlightGroup(OptionsGroup highlight_group) {
  // TODO(mattm): implement group highlighting
}

///////////////////////////////////////////////////////////////////////////////
// GeneralPageGtk, private:

GtkWidget* GeneralPageGtk::InitStartupGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  startup_homepage_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB).c_str());
  g_signal_connect(startup_homepage_radio_, "toggled",
                   G_CALLBACK(OnStartupRadioToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_homepage_radio_, FALSE, FALSE, 0);

  startup_last_session_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION).c_str());
  g_signal_connect(startup_last_session_radio_, "toggled",
                   G_CALLBACK(OnStartupRadioToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_last_session_radio_,
                     FALSE, FALSE, 0);

  startup_custom_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(startup_homepage_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_SHOW_PAGES).c_str());
  g_signal_connect(startup_custom_radio_, "toggled",
                   G_CALLBACK(OnStartupRadioToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(vbox), startup_custom_radio_, FALSE, FALSE, 0);

  GtkWidget* url_list_container = gtk_hbox_new(FALSE,
                                               gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), url_list_container, TRUE, TRUE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(url_list_container),
                    scroll_window);
  startup_custom_pages_store_ = gtk_list_store_new(COL_COUNT,
                                                   GDK_TYPE_PIXBUF,
                                                   G_TYPE_STRING,
                                                   G_TYPE_STRING);
  startup_custom_pages_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(startup_custom_pages_store_));
  gtk_container_add(GTK_CONTAINER(scroll_window), startup_custom_pages_tree_);

  // Release |startup_custom_pages_store_| so that |startup_custom_pages_tree_|
  // owns the model.
  g_object_unref(startup_custom_pages_store_);

  gtk_tree_view_set_tooltip_column(GTK_TREE_VIEW(startup_custom_pages_tree_),
                                   COL_TOOLTIP);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(startup_custom_pages_tree_),
                                    FALSE);
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_FAVICON);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", COL_URL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(startup_custom_pages_tree_),
                              column);
  startup_custom_pages_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(startup_custom_pages_tree_));
  gtk_tree_selection_set_mode(startup_custom_pages_selection_,
                              GTK_SELECTION_MULTIPLE);
  g_signal_connect(startup_custom_pages_selection_, "changed",
                   G_CALLBACK(OnStartupPagesSelectionChangedThunk), this);

  startup_custom_pages_table_model_.reset(
      new CustomHomePagesTableModel(profile()));
  startup_custom_pages_table_adapter_.reset(
      new gtk_tree::TableAdapter(this, startup_custom_pages_store_,
                                 startup_custom_pages_table_model_.get()));

  GtkWidget* url_list_buttons = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(url_list_container), url_list_buttons,
                   FALSE, FALSE, 0);

  startup_add_custom_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_ADD_BUTTON)).c_str());
  g_signal_connect(startup_add_custom_page_button_, "clicked",
                   G_CALLBACK(OnStartupAddCustomPageClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons), startup_add_custom_page_button_,
                     FALSE, FALSE, 0);
  startup_remove_custom_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
        l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_REMOVE_BUTTON)).c_str());
  g_signal_connect(startup_remove_custom_page_button_, "clicked",
                   G_CALLBACK(OnStartupRemoveCustomPageClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons),
                     startup_remove_custom_page_button_, FALSE, FALSE, 0);
  startup_use_current_page_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_OPTIONS_STARTUP_USE_CURRENT)).c_str());
  g_signal_connect(startup_use_current_page_button_, "clicked",
                   G_CALLBACK(OnStartupUseCurrentPageClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(url_list_buttons),
                     startup_use_current_page_button_, FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* GeneralPageGtk::InitHomepageGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  homepage_use_newtab_radio_ = gtk_radio_button_new_with_label(NULL,
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB).c_str());
  g_signal_connect(homepage_use_newtab_radio_, "toggled",
                   G_CALLBACK(OnNewTabIsHomePageToggledThunk), this);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_use_newtab_radio_);

  GtkWidget* homepage_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_hbox);

  homepage_use_url_radio_ = gtk_radio_button_new_with_label_from_widget(
      GTK_RADIO_BUTTON(homepage_use_newtab_radio_),
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_USE_URL).c_str());
  g_signal_connect(homepage_use_url_radio_, "toggled",
                   G_CALLBACK(OnNewTabIsHomePageToggledThunk), this);
  gtk_box_pack_start(GTK_BOX(homepage_hbox), homepage_use_url_radio_,
                     FALSE, FALSE, 0);

  homepage_use_url_entry_ = gtk_entry_new();
  g_signal_connect(homepage_use_url_entry_, "changed",
                   G_CALLBACK(OnHomepageUseUrlEntryChangedThunk), this);
  gtk_box_pack_start(GTK_BOX(homepage_hbox), homepage_use_url_entry_,
                     TRUE, TRUE, 0);

  homepage_show_home_button_checkbox_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON).c_str());
  g_signal_connect(homepage_show_home_button_checkbox_, "toggled",
                   G_CALLBACK(OnShowHomeButtonToggledThunk), this);
  gtk_container_add(GTK_CONTAINER(vbox), homepage_show_home_button_checkbox_);

  return vbox;
}

GtkWidget* GeneralPageGtk::InitDefaultSearchGroup() {
  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);

  default_search_engines_model_ = gtk_list_store_new(SEARCH_ENGINES_COL_COUNT,
                                                     G_TYPE_UINT,
                                                     G_TYPE_STRING);
  default_search_engine_combobox_ = gtk_combo_box_new_with_model(
      GTK_TREE_MODEL(default_search_engines_model_));
  g_object_unref(default_search_engines_model_);
  g_signal_connect(default_search_engine_combobox_, "changed",
                   G_CALLBACK(OnDefaultSearchEngineChangedThunk), this);
  gtk_container_add(GTK_CONTAINER(hbox), default_search_engine_combobox_);
  accessible_widget_helper_->SetWidgetName(
      default_search_engine_combobox_, IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME);

  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(default_search_engine_combobox_),
                             renderer, TRUE);
  gtk_cell_layout_set_attributes(
      GTK_CELL_LAYOUT(default_search_engine_combobox_), renderer,
      "text", SEARCH_ENGINES_COL_TITLE,
      NULL);

  template_url_model_ = profile()->GetTemplateURLModel();
  if (template_url_model_) {
    template_url_model_->Load();
    template_url_model_->AddObserver(this);
  }
  OnTemplateURLModelChanged();

  default_search_manage_engines_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(
          IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK).c_str());
  g_signal_connect(default_search_manage_engines_button_, "clicked",
                   G_CALLBACK(OnDefaultSearchManageEnginesClickedThunk), this);
  gtk_box_pack_end(GTK_BOX(hbox), default_search_manage_engines_button_,
                   FALSE, FALSE, 0);

  return hbox;
}

GtkWidget* GeneralPageGtk::InitDefaultBrowserGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // TODO(mattm): the label should be created with a text like "checking for
  // default" to be displayed while we wait for the check to complete.
  default_browser_status_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(GTK_BOX(vbox), default_browser_status_label_,
                     FALSE, FALSE, 0);

  default_browser_use_as_default_button_ = gtk_button_new_with_label(
      l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
          l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)).c_str());
  g_signal_connect(default_browser_use_as_default_button_, "clicked",
                   G_CALLBACK(OnBrowserUseAsDefaultClickedThunk), this);

  gtk_box_pack_start(GTK_BOX(vbox), default_browser_use_as_default_button_,
                     FALSE, FALSE, 0);

  GtkWidget* vbox_alignment = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(vbox_alignment), vbox);

  default_browser_worker_->StartCheckDefaultBrowser();

  return vbox_alignment;
}

void GeneralPageGtk::OnStartupRadioToggled(GtkWidget* toggle_button) {
  if (initializing_)
    return;

  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
    // When selecting a radio button, we get two signals (one for the old radio
    // being toggled off, one for the new one being toggled on.)  Ignore the
    // signal for toggling off the old button.
    return;
  }
  SaveStartupPref();
  if (toggle_button == startup_homepage_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Startup_Homepage"),
                            profile()->GetPrefs());
  } else if (toggle_button == startup_last_session_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Startup_LastSession"),
                            profile()->GetPrefs());
  } else if (toggle_button == startup_custom_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Startup_Custom"),
                            profile()->GetPrefs());
  }
}

void GeneralPageGtk::OnStartupAddCustomPageClicked(GtkWidget* button) {
  new UrlPickerDialogGtk(
      NewCallback(this, &GeneralPageGtk::OnAddCustomUrl),
      profile(),
      GTK_WINDOW(gtk_widget_get_toplevel(page_)));
}

void GeneralPageGtk::OnStartupRemoveCustomPageClicked(GtkWidget* button) {
  RemoveSelectedCustomUrls();
}

void GeneralPageGtk::OnStartupUseCurrentPageClicked(GtkWidget* button) {
  SetCustomUrlListFromCurrentPages();
}

void GeneralPageGtk::OnStartupPagesSelectionChanged(
    GtkTreeSelection* selection) {
  EnableCustomHomepagesControls(true);
}

void GeneralPageGtk::OnNewTabIsHomePageToggled(GtkWidget* toggle_button) {
  if (initializing_)
    return;
  if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button))) {
    // Ignore the signal for toggling off the old button.
    return;
  }
  if (toggle_button == homepage_use_newtab_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Homepage_UseNewTab"),
                            profile()->GetPrefs());
    UpdateHomepagePrefs();
    EnableHomepageURLField(false);
  } else if (toggle_button == homepage_use_url_radio_) {
    UserMetricsRecordAction(UserMetricsAction("Options_Homepage_UseURL"),
                            profile()->GetPrefs());
    UpdateHomepagePrefs();
    EnableHomepageURLField(true);
  }
}

void GeneralPageGtk::OnHomepageUseUrlEntryChanged(GtkWidget* editable) {
  if (initializing_)
    return;
  UpdateHomepagePrefs();
}

void GeneralPageGtk::OnShowHomeButtonToggled(GtkWidget* toggle_button) {
  if (initializing_)
    return;
  bool enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button));
  show_home_button_.SetValue(enabled);
  if (enabled) {
    UserMetricsRecordAction(
        UserMetricsAction("Options_Homepage_ShowHomeButton"),
        profile()->GetPrefs());
  } else {
    UserMetricsRecordAction(
        UserMetricsAction("Options_Homepage_HideHomeButton"),
        profile()->GetPrefs());
  }
}

void GeneralPageGtk::OnDefaultSearchEngineChanged(GtkWidget* combo_box) {
  if (default_search_initializing_)
    return;
  SetDefaultSearchEngineFromComboBox();
}

void GeneralPageGtk::OnDefaultSearchManageEnginesClicked(GtkWidget* button) {
  KeywordEditorView::Show(profile());
}

void GeneralPageGtk::OnBrowserUseAsDefaultClicked(GtkWidget* button) {
  default_browser_worker_->StartSetAsDefaultBrowser();
  // If the user made Chrome the default browser, then he/she arguably wants
  // to be notified when that changes.
  profile()->GetPrefs()->SetBoolean(prefs::kCheckDefaultBrowser, true);
  UserMetricsRecordAction(UserMetricsAction("Options_SetAsDefaultBrowser"),
                          profile()->GetPrefs());
}

void GeneralPageGtk::SaveStartupPref() {
  SessionStartupPref pref;

  if (gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(startup_last_session_radio_))) {
    pref.type = SessionStartupPref::LAST;
  } else if (gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(startup_custom_radio_))) {
    pref.type = SessionStartupPref::URLS;
  }

  pref.urls = startup_custom_pages_table_model_->GetURLs();

  SessionStartupPref::SetStartupPref(profile()->GetPrefs(), pref);
}

void GeneralPageGtk::SetColumnValues(int row, GtkTreeIter* iter) {
  SkBitmap bitmap = startup_custom_pages_table_model_->GetIcon(row);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  std::wstring text = startup_custom_pages_table_model_->GetText(row, 0);
  std::string tooltip =
      WideToUTF8(startup_custom_pages_table_model_->GetTooltip(row));
  gchar* escaped_tooltip = g_markup_escape_text(tooltip.c_str(),
                                                tooltip.size());
  gtk_list_store_set(startup_custom_pages_store_, iter,
                     COL_FAVICON, pixbuf,
                     COL_URL, WideToUTF8(text).c_str(),
                     COL_TOOLTIP, escaped_tooltip,
                     -1);
  g_object_unref(pixbuf);
  g_free(escaped_tooltip);
}

void GeneralPageGtk::SetCustomUrlListFromCurrentPages() {
  startup_custom_pages_table_model_->SetToCurrentlyOpenPages();

  SaveStartupPref();
}

void GeneralPageGtk::OnAddCustomUrl(const GURL& url) {
  std::set<int> indices;
  gtk_tree::GetSelectedIndices(startup_custom_pages_selection_, &indices);
  int index;
  if (indices.empty())
    index = startup_custom_pages_table_model_->RowCount();
  else
    index = *indices.begin() + 1;
  startup_custom_pages_table_model_->Add(index, url);

  SaveStartupPref();

  gtk_tree::SelectAndFocusRowNum(index,
                                 GTK_TREE_VIEW(startup_custom_pages_tree_));
}

void GeneralPageGtk::RemoveSelectedCustomUrls() {
  std::set<int> indices;
  gtk_tree::GetSelectedIndices(startup_custom_pages_selection_, &indices);

  int selected_row = 0;
  for (std::set<int>::reverse_iterator i = indices.rbegin();
       i != indices.rend(); ++i) {
    startup_custom_pages_table_model_->Remove(*i);
    selected_row = *i;
  }

  SaveStartupPref();

  // Select the next row after the last row deleted, or the above item if the
  // latest item was deleted or nothing when the table doesn't have any items.
  int row_count = startup_custom_pages_table_model_->RowCount();
  if (selected_row >= row_count)
    selected_row = row_count - 1;
  if (selected_row >= 0) {
    gtk_tree::SelectAndFocusRowNum(selected_row,
                                   GTK_TREE_VIEW(startup_custom_pages_tree_));
  }
}

void GeneralPageGtk::OnTemplateURLModelChanged() {
  if (!template_url_model_ || !template_url_model_->loaded()) {
    EnableDefaultSearchEngineComboBox(false);
    return;
  }
  default_search_initializing_ = true;
  gtk_list_store_clear(default_search_engines_model_);
  const TemplateURL* default_search_provider =
      template_url_model_->GetDefaultSearchProvider();
  std::vector<const TemplateURL*> model_urls =
      template_url_model_->GetTemplateURLs();
  bool populated = false;
  for (size_t i = 0; i < model_urls.size(); ++i) {
    if (!model_urls[i]->ShowInDefaultList())
      continue;
    populated = true;
    GtkTreeIter iter;
    gtk_list_store_append(default_search_engines_model_, &iter);
    gtk_list_store_set(
        default_search_engines_model_, &iter,
        SEARCH_ENGINES_COL_INDEX, i,
        SEARCH_ENGINES_COL_TITLE,
        WideToUTF8(model_urls[i]->short_name()).c_str(),
        -1);
    if (model_urls[i] == default_search_provider) {
      gtk_combo_box_set_active_iter(
          GTK_COMBO_BOX(default_search_engine_combobox_), &iter);
    }
  }
  EnableDefaultSearchEngineComboBox(populated);
  default_search_initializing_ = false;
}

void GeneralPageGtk::SetDefaultSearchEngineFromComboBox() {
  GtkTreeIter iter;
  if (!gtk_combo_box_get_active_iter(
      GTK_COMBO_BOX(default_search_engine_combobox_), &iter)) {
    return;
  }
  guint index;
  gtk_tree_model_get(GTK_TREE_MODEL(default_search_engines_model_), &iter,
                     SEARCH_ENGINES_COL_INDEX, &index,
                     -1);
  std::vector<const TemplateURL*> model_urls =
      template_url_model_->GetTemplateURLs();
  if (index < model_urls.size())
    template_url_model_->SetDefaultSearchProvider(model_urls[index]);
  else
    NOTREACHED();
}

void GeneralPageGtk::EnableDefaultSearchEngineComboBox(bool enable) {
  gtk_widget_set_sensitive(default_search_engine_combobox_, enable);
}

void GeneralPageGtk::UpdateHomepagePrefs() {
  const GURL& homepage = URLFixerUpper::FixupURL(
      gtk_entry_get_text(GTK_ENTRY(homepage_use_url_entry_)), std::string());
  bool new_tab_page_is_home_page =
    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(homepage_use_newtab_radio_));
  if (IsNewTabUIURLString(homepage)) {
    new_tab_page_is_home_page = true;
    homepage_.SetValueIfNotManaged(std::string());
  } else if (!homepage.is_valid()) {
    new_tab_page_is_home_page = true;
    if (!homepage.has_host())
      homepage_.SetValueIfNotManaged(std::string());
  } else {
    homepage_.SetValueIfNotManaged(homepage.spec());
  }
  new_tab_page_is_home_page_.SetValueIfNotManaged(new_tab_page_is_home_page);
}

void GeneralPageGtk::UpdateHomepageIsNewTabRadio(bool homepage_is_new_tab,
                                                 bool enabled) {
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(homepage_use_newtab_radio_),
                               homepage_is_new_tab);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(homepage_use_url_radio_),
                               !homepage_is_new_tab);
  gtk_widget_set_sensitive(homepage_use_newtab_radio_, enabled);
  gtk_widget_set_sensitive(homepage_use_url_radio_, enabled);
}

void GeneralPageGtk::EnableHomepageURLField(bool enabled) {
  if (homepage_.IsManaged()) {
    enabled = false;
  }
  gtk_widget_set_sensitive(homepage_use_url_entry_, enabled);
}

void GeneralPageGtk::EnableCustomHomepagesControls(bool enable) {
  gtk_widget_set_sensitive(startup_add_custom_page_button_, enable);
  gtk_widget_set_sensitive(startup_remove_custom_page_button_,
      enable &&
      gtk_tree_selection_count_selected_rows(startup_custom_pages_selection_));
  gtk_widget_set_sensitive(startup_use_current_page_button_, enable);
  gtk_widget_set_sensitive(startup_custom_pages_tree_, enable);
}

void GeneralPageGtk::SetDefaultBrowserUIState(
    ShellIntegration::DefaultBrowserUIState state) {
  const char* color = NULL;
  std::string text;
  if (state == ShellIntegration::STATE_IS_DEFAULT) {
    color = kDefaultBrowserLabelColor;
    text = l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_DEFAULT,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  } else if (state == ShellIntegration::STATE_NOT_DEFAULT) {
    color = kNotDefaultBrowserLabelColor;
    text = l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  } else if (state == ShellIntegration::STATE_UNKNOWN) {
    color = kNotDefaultBrowserLabelColor;
    text = l10n_util::GetStringFUTF8(IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
        l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
  }
  if (color) {
    char* markup = g_markup_printf_escaped(kDefaultBrowserLabelMarkup,
                                           color, text.c_str());
    gtk_label_set_markup(GTK_LABEL(default_browser_status_label_), markup);
    g_free(markup);
  }

  gtk_widget_set_sensitive(default_browser_use_as_default_button_,
                           state == ShellIntegration::STATE_NOT_DEFAULT);
}
