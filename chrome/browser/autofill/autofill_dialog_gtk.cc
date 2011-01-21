// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/form_group.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "gfx/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/l10n/l10n_util.h"

// Shows the editor for adding/editing an AutoFillProfile. If
// |auto_fill_profile| is NULL, a new AutoFillProfile should be created.
void ShowAutoFillProfileEditor(gfx::NativeView parent,
                               AutoFillDialogObserver* observer,
                               Profile* profile,
                               AutoFillProfile* auto_fill_profile);

// Shows the editor for adding/editing a CreditCard. If |credit_card| is NULL, a
// new CreditCard should be created.
void ShowAutoFillCreditCardEditor(gfx::NativeView parent,
                                  AutoFillDialogObserver* observer,
                                  Profile* profile,
                                  CreditCard* credit_card);

namespace {

// The resource id for the 'About Autofill' link button.
const gint kAutoFillDialogAboutLink = 1;

////////////////////////////////////////////////////////////////////////////////
// AutoFillDialog
//
// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog : public PersonalDataManager::Observer,
                       public NotificationObserver {
 public:
  // Identifiers for columns in the store.
  enum Column {
    COL_TITLE = 0,
    COL_IS_HEADER,
    COL_IS_SEPARATOR,  // Identifies an empty row used to reserve visual space.
    COL_WEIGHT,
    COL_WEIGHT_SET,
    COL_COUNT
  };

  // Used to identify the selection. See GetSelectionType.
  enum SelectionType {
    // Nothing is selected.
    SELECTION_EMPTY  = 0,

    // At least one header/separator row is selected.
    SELECTION_HEADER = 1 << 0,

    // At least one non-header/separator row is selected.
    SELECTION_SINGLE = 1 << 1,

    // Multiple non-header/separator rows are selected.
    SELECTION_MULTI  = 1 << 2,
  };

  AutoFillDialog(Profile* profile, AutoFillDialogObserver* observer);
  ~AutoFillDialog();

  // PersonalDataManager::Observer implementation:
  void OnPersonalDataLoaded();
  void OnPersonalDataChanged();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  Calls DeleteSoon on the global singleton dialog
  // object.
  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnDestroy);

  // 'response' signal handler.  Notifies the AutoFillDialogObserver that new
  // data is available if the response is GTK_RESPONSE_APPLY or GTK_RESPONSE_OK.
  // We close the dialog if the response is GTK_RESPONSE_OK or
  // GTK_RESPONSE_CANCEL.
  CHROMEG_CALLBACK_1(AutoFillDialog, void, OnResponse, GtkDialog*, gint);

  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnAutoFillCheckToggled);
  CHROMEG_CALLBACK_2(AutoFillDialog, void, OnRowActivated, GtkTreeView*,
                     GtkTreePath*, GtkTreeViewColumn*);
  CHROMEG_CALLBACK_0(AutoFillDialog, void, OnSelectionChanged,
                     GtkTreeSelection*);
  CHROMEG_CALLBACK_1(AutoFillDialog, gboolean, OnCheckRowIsSeparator,
                     GtkTreeModel*, GtkTreeIter*);
  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnAddAddress);
  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnAddCreditCard);
  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnEdit);
  CHROMEGTK_CALLBACK_0(AutoFillDialog, void, OnRemove);
  CHROMEG_CALLBACK_3(AutoFillDialog, gboolean, OnSelectionFilter,
                     GtkTreeSelection*, GtkTreeModel*, GtkTreePath*, gboolean);

  // Opens the 'Learn more' link in a new foreground tab.
  void OnLinkActivated();

  // Loads AutoFill profiles and credit cards using the PersonalDataManager.
  void LoadAutoFillData();

  // Creates the dialog UI widgets.
  void InitializeWidgets();

  // Updates the state of the various widgets dependant upon the state of the
  // selection, loaded state and whether AutoFill is enabled.
  void UpdateWidgetState();

  // Returns a bitmask of the selection types.
  int GetSelectionType();

  void AddAddressToTree(const AutoFillProfile& profile, GtkTreeIter* iter);

  void AddCreditCardToTree(const CreditCard& credit_card, GtkTreeIter* iter);

  // Returns the set of selected profiles and cards. The values placed in
  // the specified vectors are owned by PersonalDataManager.
  void GetSelectedEntries(std::vector<AutoFillProfile*>* profiles,
                          std::vector<CreditCard*>* cards);

  Profile* profile_;

  // Our observer.  May not be NULL.
  AutoFillDialogObserver* observer_;

  // The personal data manager, used to load AutoFill profiles and credit cards.
  // Unowned pointer, may not be NULL.
  PersonalDataManager* personal_data_;

  // Number of profiles we're displaying.
  int profile_count_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  BooleanPrefMember enable_form_autofill_;

  GtkWidget* form_autofill_enable_check_;

  // Displays the addresses then credit cards.
  GtkListStore* list_store_;

  // Displays the list_store_.
  GtkWidget* tree_;

  GtkWidget* add_address_button_;
  GtkWidget* add_credit_card_button_;
  GtkWidget* edit_button_;
  GtkWidget* remove_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(Profile* profile,
                               AutoFillDialogObserver* observer)
    : profile_(profile),
      observer_(observer),
      personal_data_(profile->GetPersonalDataManager()),
      profile_count_(0) {
  DCHECK(observer_);
  DCHECK(personal_data_);

  enable_form_autofill_.Init(prefs::kAutoFillEnabled, profile->GetPrefs(),
                             this);

  personal_data_->SetObserver(this);

  InitializeWidgets();
  LoadAutoFillData();

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
                                        IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                        IDS_AUTOFILL_DIALOG_HEIGHT_LINES,
                                        true);
}

AutoFillDialog::~AutoFillDialog() {
  // Removes observer if we are observing Profile load. Does nothing otherwise.
  personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutoFillDialog::OnPersonalDataLoaded() {
  LoadAutoFillData();
}

void AutoFillDialog::OnPersonalDataChanged() {
  LoadAutoFillData();
}

void AutoFillDialog::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::PREF_CHANGED, type.value);
  const std::string* pref_name = Details<std::string>(details).ptr();
  if (!pref_name || *pref_name == prefs::kAutoFillEnabled) {
    gtk_toggle_button_set_active(
          GTK_TOGGLE_BUTTON(form_autofill_enable_check_),
          enable_form_autofill_.GetValue() ? TRUE : FALSE);
    UpdateWidgetState();
  }
}

void AutoFillDialog::Show() {
  gtk_util::PresentWindow(dialog_, gtk_get_current_event_time());
}

void AutoFillDialog::OnDestroy(GtkWidget* widget) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void AutoFillDialog::OnResponse(GtkDialog* dialog, gint response_id) {
  if (response_id == GTK_RESPONSE_OK ||
      response_id == GTK_RESPONSE_CANCEL ||
      response_id == GTK_RESPONSE_DELETE_EVENT) {
    gtk_widget_destroy(GTK_WIDGET(dialog));
  }

  if (response_id == kAutoFillDialogAboutLink)
    OnLinkActivated();
}

void AutoFillDialog::OnAutoFillCheckToggled(GtkWidget* widget) {
  bool enabled = gtk_toggle_button_get_active(
      GTK_TOGGLE_BUTTON(form_autofill_enable_check_));
  if (enabled) {
    UserMetrics::RecordAction(UserMetricsAction("Options_FormAutofill_Enable"),
                              profile_);
  } else {
    UserMetrics::RecordAction(UserMetricsAction("Options_FormAutofill_Disable"),
                              profile_);
  }
  enable_form_autofill_.SetValueIfNotManaged(enabled);
  profile_->GetPrefs()->ScheduleSavePersistentPrefs();
  UpdateWidgetState();
}

void AutoFillDialog::OnRowActivated(GtkTreeView* tree_view,
                                    GtkTreePath* path,
                                    GtkTreeViewColumn* column) {
  if (GetSelectionType() == SELECTION_SINGLE)
    OnEdit(NULL);
}

void AutoFillDialog::OnSelectionChanged(GtkTreeSelection* selection) {
  UpdateWidgetState();
}

gboolean AutoFillDialog::OnCheckRowIsSeparator(GtkTreeModel* model,
                                               GtkTreeIter* iter) {
  gboolean is_separator;
  gtk_tree_model_get(model, iter, COL_IS_SEPARATOR, &is_separator, -1);
  return is_separator;
}

void AutoFillDialog::OnAddAddress(GtkWidget* widget) {
  ShowAutoFillProfileEditor(NULL, observer_, profile_, NULL);
}

void AutoFillDialog::OnAddCreditCard(GtkWidget* widget) {
  ShowAutoFillCreditCardEditor(NULL, observer_, profile_, NULL);
}

void AutoFillDialog::OnEdit(GtkWidget* widget) {
  DCHECK_EQ(SELECTION_SINGLE, GetSelectionType());

  std::vector<AutoFillProfile*> profiles;
  std::vector<CreditCard*> cards;

  GetSelectedEntries(&profiles, &cards);

  if (profiles.size() == 1)
    ShowAutoFillProfileEditor(dialog_, observer_, profile_, profiles[0]);
  else if (cards.size() == 1)
    ShowAutoFillCreditCardEditor(dialog_, observer_, profile_, cards[0]);
}

void AutoFillDialog::OnRemove(GtkWidget* widget) {
  PersonalDataManager* data_manager = profile_->GetPersonalDataManager();
  std::vector<AutoFillProfile*> selected_profiles;
  std::vector<CreditCard*> selected_cards;

  GetSelectedEntries(&selected_profiles, &selected_cards);

  std::vector<AutoFillProfile> profiles;
  for (std::vector<AutoFillProfile*>::const_iterator i =
           data_manager->profiles().begin();
       i != data_manager->profiles().end(); ++i) {
    if (std::find(selected_profiles.begin(), selected_profiles.end(), *i) ==
        selected_profiles.end()) {
      profiles.push_back(**i);
    }
  }

  std::vector<CreditCard> cards;
  for (std::vector<CreditCard*>::const_iterator i =
           data_manager->credit_cards().begin();
       i != data_manager->credit_cards().end(); ++i) {
    if (std::find(selected_cards.begin(), selected_cards.end(), *i) ==
        selected_cards.end()) {
      cards.push_back(**i);
    }
  }

  observer_->OnAutoFillDialogApply(&profiles, &cards);
}

gboolean AutoFillDialog::OnSelectionFilter(GtkTreeSelection* selection,
                                           GtkTreeModel* model,
                                           GtkTreePath* path,
                                           gboolean path_currently_selected) {
  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter(model, &iter, path)) {
    NOTREACHED();
    return TRUE;
  }
  gboolean is_header;
  gboolean is_separator;
  gtk_tree_model_get(model, &iter, COL_IS_HEADER, &is_header,
                     COL_IS_SEPARATOR, &is_separator, -1);
  return !is_header && !is_separator;
}

void AutoFillDialog::OnLinkActivated() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || !browser->GetSelectedTabContents())
    browser = Browser::Create(profile_);
  browser->OpenAutoFillHelpTabAndActivate();
}

void AutoFillDialog::LoadAutoFillData() {
  if (!personal_data_->IsDataLoaded()) {
    UpdateWidgetState();
    return;
  }

  // Rebuild the underlying store.
  gtk_list_store_clear(list_store_);

  GtkTreeIter iter;
  // Address title.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_WEIGHT, PANGO_WEIGHT_BOLD,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE,
          l10n_util::GetStringUTF8(IDS_AUTOFILL_ADDRESSES_GROUP_NAME).c_str(),
      COL_IS_HEADER, TRUE,
      -1);
  // Address separator.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_SEPARATOR, TRUE,
      -1);

  // The addresses.
  profile_count_ = 0;
  for (std::vector<AutoFillProfile*>::const_iterator i =
           personal_data_->profiles().begin();
       i != personal_data_->profiles().end(); ++i) {
    AddAddressToTree(*(*i), &iter);
    profile_count_++;
  }

  // Blank row between addresses and credit cards.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_HEADER, TRUE,
      -1);

  // Credit card title.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_WEIGHT, PANGO_WEIGHT_BOLD,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE,
          l10n_util::GetStringUTF8(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME).c_str(),
      COL_IS_HEADER, TRUE,
      -1);
  // Credit card separator.
  gtk_list_store_append(list_store_, &iter);
  gtk_list_store_set(
      list_store_, &iter,
      COL_IS_SEPARATOR, TRUE,
      -1);

  // The credit cards.
  for (std::vector<CreditCard*>::const_iterator i =
           personal_data_->credit_cards().begin();
       i != personal_data_->credit_cards().end(); ++i) {
    AddCreditCardToTree(*(*i), &iter);
  }

  UpdateWidgetState();
}

void AutoFillDialog::InitializeWidgets() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_OPTIONS_TITLE).c_str(),
      // AutoFill dialog is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);

  GtkBox* vbox = GTK_BOX(GTK_DIALOG(dialog_)->vbox);
  gtk_box_set_spacing(vbox, gtk_util::kControlSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroyThunk), this);

  form_autofill_enable_check_ = gtk_check_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OPTIONS_AUTOFILL_ENABLE).c_str());
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(form_autofill_enable_check_),
                               enable_form_autofill_.GetValue());
  g_signal_connect(G_OBJECT(form_autofill_enable_check_), "toggled",
                   G_CALLBACK(OnAutoFillCheckToggledThunk), this);
  gtk_box_pack_start(vbox, form_autofill_enable_check_, FALSE, FALSE, 0);

  // Allow the contents to be scrolled.
  GtkWidget* scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
                                      GTK_SHADOW_ETCHED_IN);

  list_store_ = gtk_list_store_new(COL_COUNT,
                                   G_TYPE_STRING,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_INT,
                                   G_TYPE_BOOLEAN);
  tree_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store_));
  g_object_unref(list_store_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_), FALSE);
  gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(tree_),
                                       OnCheckRowIsSeparatorThunk, this, NULL);
  g_signal_connect(tree_, "row-activated", G_CALLBACK(OnRowActivatedThunk),
                   this);
  gtk_container_add(GTK_CONTAINER(scrolled_window), tree_);

  GtkWidget* h_box1 = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(vbox, h_box1, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(h_box1), scrolled_window, TRUE, TRUE, 0);

  GtkWidget* controls_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(h_box1), controls_box, FALSE, FALSE, 0);

  add_address_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ADD_ADDRESS_BUTTON).c_str());
  g_signal_connect(add_address_button_, "clicked",
                   G_CALLBACK(OnAddAddressThunk), this);
  gtk_box_pack_start(GTK_BOX(controls_box), add_address_button_, FALSE, FALSE,
                     0);

  add_credit_card_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON).c_str());
  g_signal_connect(add_credit_card_button_, "clicked",
                   G_CALLBACK(OnAddCreditCardThunk), this);
  gtk_box_pack_start(GTK_BOX(controls_box), add_credit_card_button_, FALSE,
                     FALSE, 0);

  edit_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_EDIT_BUTTON).c_str());
  g_signal_connect(edit_button_, "clicked", G_CALLBACK(OnEditThunk), this);
  gtk_box_pack_start(GTK_BOX(controls_box), edit_button_, FALSE, FALSE, 0);

  remove_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DELETE_BUTTON).c_str());
  g_signal_connect(remove_button_, "clicked", G_CALLBACK(OnRemoveThunk), this);
  gtk_box_pack_start(GTK_BOX(controls_box), remove_button_, FALSE, FALSE, 0);

  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      "",
      gtk_cell_renderer_text_new(),
      "text", COL_TITLE,
      "weight", COL_WEIGHT,
      "weight-set", COL_WEIGHT_SET,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), column);

  GtkTreeSelection* selection =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
  gtk_tree_selection_set_select_function(selection, OnSelectionFilterThunk,
                                         this, NULL);
  g_signal_connect(selection, "changed", G_CALLBACK(OnSelectionChangedThunk),
                   this);

  GtkWidget* link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_HELP_LABEL).c_str());
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog_), link,
                               kAutoFillDialogAboutLink);

  // Setting the link widget to secondary positions the button on the left side
  // of the action area (vice versa for RTL layout).
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area), link, TRUE);
}

void AutoFillDialog::UpdateWidgetState() {
  gtk_widget_set_sensitive(form_autofill_enable_check_,
                           !enable_form_autofill_.IsManaged());
  if (!personal_data_->IsDataLoaded() || !enable_form_autofill_.GetValue()) {
    gtk_widget_set_sensitive(add_address_button_, FALSE);
    gtk_widget_set_sensitive(add_credit_card_button_, FALSE);
    gtk_widget_set_sensitive(edit_button_, FALSE);
    gtk_widget_set_sensitive(remove_button_, FALSE);
    gtk_widget_set_sensitive(tree_, FALSE);
  } else {
    gtk_widget_set_sensitive(add_address_button_, TRUE);
    gtk_widget_set_sensitive(add_credit_card_button_, TRUE);
    int selection_type = GetSelectionType();
    gtk_widget_set_sensitive(edit_button_, selection_type == SELECTION_SINGLE);
    // Enable the remove button if at least one non-header row is selected.
    gtk_widget_set_sensitive(remove_button_,
                             (selection_type & SELECTION_SINGLE) != 0);
    gtk_widget_set_sensitive(tree_, TRUE);
  }
}

static void RowIteratorFunction(GtkTreeModel* model,
                                GtkTreePath* path,
                                GtkTreeIter* iter,
                                gpointer data) {
  int* type = reinterpret_cast<int*>(data);
  bool is_header = false;
  GValue value = { 0 };
  gtk_tree_model_get_value(model, iter, AutoFillDialog::COL_IS_HEADER, &value);
  is_header = g_value_get_boolean(&value);
  g_value_unset(&value);

  if (!is_header) {
    // Is it a separator?
    GValue value = { 0 };
    gtk_tree_model_get_value(model, iter, AutoFillDialog::COL_IS_SEPARATOR,
                             &value);
    is_header = g_value_get_boolean(&value);
    g_value_unset(&value);
  }

  if (is_header) {
    *type |= AutoFillDialog::SELECTION_HEADER;
  } else {
    if ((*type & AutoFillDialog::SELECTION_SINGLE) == 0)
      *type |= AutoFillDialog::SELECTION_SINGLE;
    else
      *type |= AutoFillDialog::SELECTION_MULTI;
  }
}

int AutoFillDialog::GetSelectionType() {
  int state = SELECTION_EMPTY;
  gtk_tree_selection_selected_foreach(
      gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_)), RowIteratorFunction,
      &state);
  return state;
}

void AutoFillDialog::AddAddressToTree(const AutoFillProfile& profile,
                                      GtkTreeIter* iter) {
  gtk_list_store_append(list_store_, iter);
  gtk_list_store_set(
      list_store_, iter,
      COL_WEIGHT, PANGO_WEIGHT_NORMAL,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE, UTF16ToUTF8(profile.Label()).c_str(),
      -1);
}

void AutoFillDialog::AddCreditCardToTree(const CreditCard& credit_card,
                                         GtkTreeIter* iter) {
  gtk_list_store_append(list_store_, iter);
  gtk_list_store_set(
      list_store_, iter,
      COL_WEIGHT, PANGO_WEIGHT_NORMAL,
      COL_WEIGHT_SET, TRUE,
      COL_TITLE, UTF16ToUTF8(credit_card.PreviewSummary()).c_str(),
      -1);
}

void AutoFillDialog::GetSelectedEntries(
    std::vector<AutoFillProfile*>* profiles,
    std::vector<CreditCard*>* cards) {
  std::set<int> selection;
  gtk_tree::GetSelectedIndices(
      gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_)), &selection);

  for (std::set<int>::const_iterator i = selection.begin();
       i != selection.end(); ++i) {
    // 2 is the number of header rows.
    int index = *i - 2;
    if (index >= 0 &&
        index < static_cast<int>(personal_data_->profiles().size())) {
      profiles->push_back(personal_data_->profiles()[index]);
      continue;
    }

    // Empty row, header and separator are next.
    index -= profile_count_ + 3;
    if (index >= 0 && index <
        static_cast<int>(personal_data_->credit_cards().size())) {
      cards->push_back(personal_data_->credit_cards()[index]);
    }
  }
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile) {
  DCHECK(profile);

  if (!dialog)
    dialog = new AutoFillDialog(profile, observer);
  dialog->Show();
}
