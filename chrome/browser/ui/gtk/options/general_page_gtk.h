// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/options/managed_prefs_banner_gtk.h"
#include "chrome/browser/ui/options/options_page_base.h"
#include "googleurl/src/gurl.h"
#include "ui/base/gtk/gtk_signal.h"

class AccessibleWidgetHelper;
class CustomHomePagesTableModel;
class Profile;
class TemplateURLModel;

class GeneralPageGtk : public OptionsPageBase,
                       public TemplateURLModelObserver,
                       public ShellIntegration::DefaultBrowserObserver,
                       public gtk_tree::TableAdapter::Delegate {
 public:
  explicit GeneralPageGtk(Profile* profile);
  ~GeneralPageGtk();

  GtkWidget* get_page_widget() const { return page_; }

 private:
  GtkWindow* GetWindow();

  // Overridden from OptionsPageBase
  virtual void NotifyPrefChanged(const std::string* pref_name);
  virtual void HighlightGroup(OptionsGroup highlight_group);

  // Initialize the option group widgets, return their container
  GtkWidget* InitStartupGroup();
  GtkWidget* InitHomepageGroup();
  GtkWidget* InitDefaultSearchGroup();
  GtkWidget* InitDefaultBrowserGroup();

  // Saves the startup preference from the values in the ui
  void SaveStartupPref();

  // Set the custom url list using the pages currently open
  void SetCustomUrlListFromCurrentPages();

  // Callback from UrlPickerDialogGtk, for adding custom urls manually.
  // If a single row in the list is selected, the new url will be inserted
  // before that row.  Otherwise the new row will be added to the end.
  void OnAddCustomUrl(const GURL& url);

  // Removes urls that are currently selected
  void RemoveSelectedCustomUrls();

  // Overridden from TemplateURLModelObserver.
  // Populates the default search engine combobox from the model.
  virtual void OnTemplateURLModelChanged();

  // Set the default search engine pref to the combo box active item.
  void SetDefaultSearchEngineFromComboBox();

  // Set the default search engine combo box state.
  void EnableDefaultSearchEngineComboBox(bool enable);

  // Copies the home page preferences from the gui controls to
  // kNewTabPageIsHomePage and kHomePage. If an empty or null-host
  // URL is specified, then we revert to using NewTab page as the Homepage.
  void UpdateHomepagePrefs();

  // Enables or disables the field for entering a custom homepage URL.
  void EnableHomepageURLField(bool enabled);

  // Sets the state and enables/disables the radio buttons that control
  // if the home page is the new tab page.
  void UpdateHomepageIsNewTabRadio(bool homepage_is_new_tab, bool enabled);

  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupRadioToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupAddCustomPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupRemoveCustomPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnStartupUseCurrentPageClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnNewTabIsHomePageToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnHomepageUseUrlEntryChanged);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnShowHomeButtonToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnDefaultSearchEngineChanged);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void,
                       OnDefaultSearchManageEnginesClicked);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnInstantToggled);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnBrowserUseAsDefaultClicked);
  CHROMEGTK_CALLBACK_1(GeneralPageGtk, void, OnInstantLabelSizeAllocate,
                       GtkAllocation*);
  CHROMEGTK_CALLBACK_0(GeneralPageGtk, void, OnSearchLearnMoreClicked);

  CHROMEG_CALLBACK_0(GeneralPageGtk, void, OnStartupPagesSelectionChanged,
                     GtkTreeSelection*);

  // Enables/Disables the controls associated with the custom start pages
  // option if that preference is not selected.
  void EnableCustomHomepagesControls(bool enable);

  // ShellIntegration::DefaultBrowserObserver implementation.
  virtual void SetDefaultBrowserUIState(
      ShellIntegration::DefaultBrowserUIState state);

  // gtk_tree::TableAdapter::Delegate implementation.
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

  // Widgets of the startup group
  GtkWidget* startup_homepage_radio_;
  GtkWidget* startup_last_session_radio_;
  GtkWidget* startup_custom_radio_;
  GtkWidget* startup_custom_pages_tree_;
  GtkListStore* startup_custom_pages_store_;
  GtkTreeSelection* startup_custom_pages_selection_;
  GtkWidget* startup_add_custom_page_button_;
  GtkWidget* startup_remove_custom_page_button_;
  GtkWidget* startup_use_current_page_button_;

  // The model for |startup_custom_pages_store_|.
  scoped_ptr<CustomHomePagesTableModel> startup_custom_pages_table_model_;
  scoped_ptr<gtk_tree::TableAdapter> startup_custom_pages_table_adapter_;

  // Widgets and prefs of the homepage group
  GtkWidget* homepage_use_newtab_radio_;
  GtkWidget* homepage_use_url_radio_;
  GtkWidget* homepage_use_url_entry_;
  GtkWidget* homepage_show_home_button_checkbox_;
  BooleanPrefMember new_tab_page_is_home_page_;
  StringPrefMember homepage_;
  BooleanPrefMember show_home_button_;

  // Widgets and data of the default search group
  GtkWidget* default_search_engine_combobox_;
  GtkListStore* default_search_engines_model_;
  GtkWidget* default_search_manage_engines_button_;
  TemplateURLModel* template_url_model_;
  GtkWidget* instant_checkbox_;
  // This widget acts as the indent for the instant warning label.
  GtkWidget* instant_indent_;
  BooleanPrefMember instant_;

  // Widgets of the default browser group
  GtkWidget* default_browser_status_label_;
  GtkWidget* default_browser_use_as_default_button_;
  BooleanPrefMember default_browser_policy_;

  // The parent GtkTable widget
  GtkWidget* page_;

  // Flag to ignore gtk callbacks while we are populating default search urls.
  bool default_search_initializing_;

  // Flag to ignore gtk callbacks while we are loading prefs, to avoid
  // then turning around and saving them again.
  bool initializing_;

  // The helper object that performs default browser set/check tasks.
  scoped_refptr<ShellIntegration::DefaultBrowserWorker> default_browser_worker_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  // Tracks managed preference warning banner state.
  ManagedPrefsBannerGtk managed_prefs_banner_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GeneralPageGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_GENERAL_PAGE_GTK_H_
