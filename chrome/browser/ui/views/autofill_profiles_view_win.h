// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_
#pragma once

#include <list>
#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/common/notification_observer.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/table_model.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/link.h"
#include "views/controls/table/table_view_observer.h"
#include "views/controls/textfield/textfield.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class GridLayout;
class ImageButton;
class Label;
class RadioButton;
class TableView;
class TextButton;
}  // namespace views

class PrefService;
class SkBitmap;

///////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView
//
//  The contents of the "AutoFill profiles" dialog window.
//
// Overview: has following sub-views:
// EditableSetViewContents - set of displayed fields for address or credit card,
//   has iterator to std::vector<EditableSetInfo> vector so data could be
//   updated or notifications passes to the dialog view.
// PhoneSubView - support view for the phone fields sets. used in
//   ScrollViewContents.
// And there is a support data structure EditableSetInfo which encapsulates
// editable set (address or credit card) and allows for quick addition and
// deletion.
class AutoFillProfilesView : public views::View,
                             public views::DialogDelegate,
                             public views::ButtonListener,
                             public views::LinkController,
                             public views::FocusChangeListener,
                             public views::TableViewObserver,
                             public PersonalDataManager::Observer,
                             public NotificationObserver {
 public:
  virtual ~AutoFillProfilesView();

  static int Show(gfx::NativeWindow parent,
                  AutoFillDialogObserver* observer,
                  PersonalDataManager* personal_data_manager,
                  Profile* profile,
                  PrefService* preferences,
                  AutoFillProfile* imported_profile,
                  CreditCard* imported_credit_card);

 protected:
  // Forward declaration. This struct defined further down.
  struct EditableSetInfo;
  // Called when 'Add Address' (|group_type| is
  // ContentListTableModel::kAddressGroup) or 'Add Credit Card' (|group_type| is
  // ContentListTableModel::kCreditCardGroup) is clicked.
  void AddClicked(int group_type);
  // Called when 'Edit...' is clicked.
  void EditClicked();
  // Called when 'Remove' is clicked.
  void DeleteClicked();
  // Called when 'Edit' dialog is accepted.
  void EditAccepted(EditableSetInfo* data, bool new_item);

  // Updates state of the buttons.
  void UpdateWidgetState();

  // Following two functions are called from opened child dialog to
  // disable/enable buttons.
  void ChildWindowOpened();
  void ChildWindowClosed();

  // Returns warning bitmap to set on warning indicator. If |good| is true it
  // returns the bitmap idicating validity, if false - indicating error.
  // Caller owns the bitmap after the call.
  SkBitmap* GetWarningBitmap(bool good);

  // views::View methods:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual View* GetExtraView();
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool CanResize() const { return true; }
  virtual bool CanMaximize() const { return false; }
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool HasAlwaysOnTopMenu() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();
  virtual bool Cancel();
  virtual bool Accept();

  // views::ButtonListener methods:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  // views::LinkController methods:
  virtual void LinkActivated(views::Link* source, int event_flags);

  // views::FocusChangeListener methods:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

  // views::TableViewObserver methods:
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();

  // PersonalDataManager::Observer methods:
  virtual void OnPersonalDataLoaded();
  virtual void OnPersonalDataChanged();

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Helper structure to keep info on one address or credit card.
  // Keeps info on one item in EditableSetViewContents.
  // Also keeps info on opened status. Allows to quickly add and delete items,
  // and then rebuild EditableSetViewContents.
  struct EditableSetInfo {
    bool is_address;
    // If |is_address| is true |address| has some data and |credit_card|
    // is empty, and vice versa
    AutoFillProfile address;
    CreditCard credit_card;

    explicit EditableSetInfo(const AutoFillProfile* input_address)
        : address(*input_address),
          is_address(true) {
    }
    explicit EditableSetInfo(const CreditCard* input_credit_card)
        : credit_card(*input_credit_card),
          is_address(false) {
    }

    std::string guid() const {
      if (is_address)
        return address.guid();
      else
        return credit_card.guid();
    }
  };

 private:
  // Indicates that there was no item focused. After re-building of the lists
  // first item will be focused.
  static const int kNoItemFocused = -1;

  struct FocusedItem {
    int group;
    int item;
    FocusedItem() : group(kNoItemFocused), item(kNoItemFocused) {}
    FocusedItem(int g, int i) : group(g), item(i) {}
  };

  AutoFillProfilesView(AutoFillDialogObserver* observer,
                       PersonalDataManager* personal_data_manager,
                       Profile* profile,
                       PrefService* preferences,
                       AutoFillProfile* imported_profile,
                       CreditCard* imported_credit_card);
  void Init();

  void GetData();
  bool IsDataReady() const;
  void SaveData();

  // Rebuilds the view by deleting and re-creating sub-views
  void RebuildView(const FocusedItem& new_focus_index);

  // PhoneSubView encapsulates three phone fields (country, area, and phone)
  // and label above them, so they could be used together in one grid cell.
  class PhoneSubView : public views::View,
                       public views::ButtonListener {
   public:
    PhoneSubView(AutoFillProfilesView* autofill_view,
                 views::Label* label,
                 views::Textfield* text_phone);
    virtual ~PhoneSubView() {}

    virtual void ContentsChanged(views::Textfield* sender,
                                 const string16& new_contents);

    bool IsValid() const;

    views::Textfield* text_phone() { return text_phone_; }

   protected:
    // views::View methods:
    virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                      views::View* child);

    // public views::ButtonListener method:
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event) {
      // Only stub is needed, it is never called.
      NOTREACHED();
    }

   private:
    void UpdateButtons();
    AutoFillProfilesView* autofill_view_;
    views::Label* label_;
    views::Textfield* text_phone_;
    views::ImageButton* phone_warning_button_;
    bool last_state_;

    DISALLOW_COPY_AND_ASSIGN(PhoneSubView);
  };

  // Forward declaration.
  class StringVectorComboboxModel;

  // Sub-view for editing/adding a credit card or address.
  class EditableSetViewContents : public views::View,
                                  public views::DialogDelegate,
                                  public views::ButtonListener,
                                  public views::Textfield::Controller,
                                  public views::Combobox::Listener {
   public:
    EditableSetViewContents(AutoFillProfilesView* observer,
                            bool new_item,
                            const EditableSetInfo& field_set);
    virtual ~EditableSetViewContents() {}

   protected:
    // views::View methods:
    virtual void Layout();
    virtual gfx::Size GetPreferredSize();
    virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                      views::View* child);

    // views::DialogDelegate methods:
    virtual int GetDialogButtons() const;
    virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
    virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
    virtual bool CanResize() const { return false; }
    virtual bool CanMaximize() const { return false; }
    virtual bool IsModal() const { return true; }
    virtual bool HasAlwaysOnTopMenu() const { return false; }
    virtual std::wstring GetWindowTitle() const;
    virtual void WindowClosing();
    virtual views::View* GetContentsView();
    virtual bool Cancel();
    virtual bool Accept();

    // views::ButtonListener methods:
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event);

    // views::Textfield::Controller methods:
    virtual void ContentsChanged(views::Textfield* sender,
                                 const string16& new_contents);
    virtual bool HandleKeyEvent(views::Textfield* sender,
                                const views::KeyEvent& keystroke);

    // views::Combobox::Listener methods:
    virtual void ItemChanged(views::Combobox* combo_box,
                             int prev_index,
                             int new_index);
   private:
    enum TextFields {
      TEXT_FULL_NAME,
      TEXT_COMPANY,
      TEXT_EMAIL,
      TEXT_ADDRESS_LINE_1,
      TEXT_ADDRESS_LINE_2,
      TEXT_ADDRESS_CITY,
      TEXT_ADDRESS_STATE,
      TEXT_ADDRESS_ZIP,
      TEXT_ADDRESS_COUNTRY,
      TEXT_PHONE_PHONE,
      TEXT_FAX_PHONE,
      TEXT_CC_NAME,
      TEXT_CC_NUMBER,
      // Must be last.
      MAX_TEXT_FIELD
    };

    void InitAddressFields(views::GridLayout* layout);
    void InitCreditCardFields(views::GridLayout* layout);
    void InitLayoutGrid(views::GridLayout* layout);
    views::Label* CreateLeftAlignedLabel(int label_id);

    void UpdateButtons();

    // If |field| is a phone or fax ContentsChanged is passed to the
    // PhoneSubView, the appropriate fields in |temporary_info_| are updated and
    // true is returned. Otherwise false is returned.
    bool UpdateContentsPhoneViews(TextFields field,
                                  views::Textfield* sender,
                                  const string16& new_contents);

    views::Textfield* text_fields_[MAX_TEXT_FIELD];
    EditableSetInfo temporary_info_;
    bool has_credit_card_number_been_edited_;
    AutoFillProfilesView* observer_;
    scoped_ptr<StringVectorComboboxModel> combo_box_model_month_;
    views::Combobox* combo_box_month_;
    scoped_ptr<StringVectorComboboxModel> combo_box_model_year_;
    views::Combobox* combo_box_year_;
    bool new_item_;
    std::vector<PhoneSubView*> phone_sub_views_;

    struct TextFieldToAutoFill {
      TextFields text_field;
      AutofillFieldType type;
    };

    static TextFieldToAutoFill address_fields_[];
    static TextFieldToAutoFill credit_card_fields_[];

    static const int double_column_fill_view_set_id_ = 0;
    static const int double_column_leading_view_set_id_ = 1;
    static const int triple_column_fill_view_set_id_ = 2;
    static const int triple_column_leading_view_set_id_ = 3;
    static const int four_column_city_state_zip_set_id_ = 4;
    static const int double_column_ccnumber_cvc_ = 5;
    static const int three_column_header_ = 6;
    static const int double_column_ccexpiration_ = 7;

    DISALLOW_COPY_AND_ASSIGN(EditableSetViewContents);
  };

  class StringVectorComboboxModel : public ui::ComboboxModel {
   public:
    StringVectorComboboxModel() {}
    virtual ~StringVectorComboboxModel() {}

    // Sets the vector of the strings for the combobox. Swaps content with
    // |source|.
    void set_cb_strings(std::vector<std::wstring> *source);

    // Overridden from ui::ComboboxModel:
    virtual int GetItemCount();
    virtual string16 GetItemAt(int index);

    // Find an index of the item in the model, -1 if not present.
    int GetIndex(const std::wstring& value);

   private:
    std::vector<std::wstring> cb_strings_;

    DISALLOW_COPY_AND_ASSIGN(StringVectorComboboxModel);
  };

  // Model for scrolling credit cards and addresses.
  class ContentListTableModel : public ui::TableModel {
   public:
    ContentListTableModel(std::vector<EditableSetInfo>* profiles,
                          std::vector<EditableSetInfo>* credit_cards);
    virtual ~ContentListTableModel() {}

    // Two constants defined for indexes of groups. The first one is index
    // of Add Address button, the second one is the index of Add Credit Card
    // button.
    static const int kAddressGroup = 1;
    static const int kCreditCardGroup = 2;

    void Refresh();
    void AddItem(int index);
    void RemoveItem(int index);
    void UpdateItem(int index);

    // ui::TableModel members:
    virtual int RowCount() OVERRIDE;
    virtual string16 GetText(int row, int column_id) OVERRIDE;
    virtual bool HasGroups() OVERRIDE { return true; }
    virtual ui::TableModel::Groups GetGroups() OVERRIDE;
    virtual int GetGroupID(int row) OVERRIDE;
    virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

   private:
    std::vector<EditableSetInfo>* profiles_;
    std::vector<EditableSetInfo>* credit_cards_;
    ui::TableModelObserver* observer_;

    DISALLOW_COPY_AND_ASSIGN(ContentListTableModel);
  };

  AutoFillDialogObserver* observer_;
  PersonalDataManager* personal_data_manager_;
  Profile* profile_;
  PrefService* preferences_;
  std::vector<EditableSetInfo> profiles_set_;
  std::vector<EditableSetInfo> credit_card_set_;

  BooleanPrefMember enable_auto_fill_;

  views::Checkbox* enable_auto_fill_button_;
  views::Button* add_address_button_;
  views::Button* add_credit_card_button_;
  views::Button* edit_button_;
  views::Button* remove_button_;
  views::TableView* scroll_view_;
  scoped_ptr<ContentListTableModel> table_model_;
  views::FocusManager* focus_manager_;
  bool child_dialog_opened_;

  static AutoFillProfilesView* instance_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillProfilesView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_
