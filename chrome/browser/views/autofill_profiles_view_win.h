// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_
#define CHROME_BROWSER_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_

#include <vector>
#include <list>

#include "app/combobox_model.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "views/controls/combobox/combobox.h"
#include "views/controls/textfield/textfield.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

namespace views {
class GridLayout;
class ImageButton;
class Label;
class ScrollView;
}

///////////////////////////////////////////////////////////////////////////////
// AutoFillProfilesView
//
//  The contents of the "Autofill profiles" dialog window.
//
// Overview: has following sub-views:
// AutoFillScrollView - scroll view that has all of the addresses and credit
//   cards. Implemented by connecting views::ScrollView to ScrollViewContents.
// ScrollViewContents - contents of the scroll view. Has multiple
//   EditableSetViewContents views for credit cards and addresses and
//   'Add address' and 'Add Credit Card' buttons.
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
                             public views::ButtonListener {
 public:
  virtual ~AutoFillProfilesView();

  static int Show(AutoFillDialogObserver* observer,
                  const std::vector<AutoFillProfile*>& profiles,
                  const std::vector<CreditCard*>& credit_cards);

 protected:
  enum EditableSetType {
    EDITABLE_SET_ADDRESS,
    EDITABLE_SET_CREDIT_CARD,
  };
  // forward declaration. This struct defined further down.
  struct EditableSetInfo;
  // Callbacks, called from EditableSetViewContents and ScrollViewContents.
  // Called from ScrollViewContents when 'Add Address' (|item_type| is
  // EDITABLE_SET_ADDRESS) or 'Add Credit Card' (|item_type| is
  // EDITABLE_SET_CREDIT_CARD) is clicked.
  void AddClicked(EditableSetType item_type);
  // Called from EditableSetViewContents when 'Delete' is clicked.
  // |field_set_iterator| is iterator to item being deleted
  void DeleteEditableSet(
      std::vector<EditableSetInfo>::iterator field_set_iterator);
  // Called from EditableSetViewContents when header of the item is clicked
  // either to collapse or expand. |field_set_iterator| is iterator to item
  // being changed.
  void CollapseStateChanged(
      std::vector<EditableSetInfo>::iterator field_set_iterator);

  // views::View methods:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                    views::View* child);

  // views::DialogDelegate methods:
  virtual int GetDialogButtons() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool CanResize() const { return true; }
  virtual bool CanMaximize() const { return true; }
  virtual bool IsAlwaysOnTop() const { return false; }
  virtual bool HasAlwaysOnTopMenu() const { return false; }
  virtual std::wstring GetWindowTitle() const;
  virtual void WindowClosing();
  virtual views::View* GetContentsView();
  virtual bool Accept();

  // views::ButtonListener methods:
  virtual void ButtonPressed(views::Button* sender,
       const views::Event& event);

  // Helper structure to keep info on one address or credit card.
  // Keeps info on one item in EditableSetViewContents.
  // Also keeps info on opened status. Allows to quickly add and delete items,
  // and then rebuild EditableSetViewContents.
  struct EditableSetInfo {
    bool is_address;
    bool is_opened;
    // If |is_address| is true |address| has some data and |credit_card|
    // is empty, and vice versa
    AutoFillProfile address;
    CreditCard credit_card;

    EditableSetInfo(const AutoFillProfile* input_address, bool opened)
        : address(*input_address),
          is_address(true),
          is_opened(opened) {
    }
    EditableSetInfo(const CreditCard* input_credit_card, bool opened)
        : credit_card(*input_credit_card),
          is_address(false),
          is_opened(opened) {
    }
  };

 private:
  AutoFillProfilesView(AutoFillDialogObserver* observer,
                       const std::vector<AutoFillProfile*>& profiles,
                       const std::vector<CreditCard*>& credit_cards);
  void Init();

  // PhoneSubView encapsulates three phone fields (country, area, and phone)
  // and label above them, so they could be used together in one grid cell.
  class PhoneSubView : public views::View {
   public:
    PhoneSubView(views::Label* label,
                 views::Textfield* text_country,
                 views::Textfield* text_area,
                 views::Textfield* text_phone);
    virtual ~PhoneSubView() {}

   protected:
    // views::View methods:
    virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                      views::View* child);
   private:
    views::Label* label_;
    views::Textfield* text_country_;
    views::Textfield* text_area_;
    views::Textfield* text_phone_;

    DISALLOW_COPY_AND_ASSIGN(PhoneSubView);
  };

  // forward declaration
  class AddressComboBoxModel;

  // Sub-view dealing with addresses.
  class EditableSetViewContents : public views::View,
                                  public views::Textfield::Controller,
                                  public views::ButtonListener,
                                  public views::Combobox::Listener {
   public:
    EditableSetViewContents(AutoFillProfilesView* observer,
                            AddressComboBoxModel* billing_model,
                            AddressComboBoxModel* shipping_model,
                            std::vector<EditableSetInfo>::iterator field_set);
    virtual ~EditableSetViewContents() {}

   protected:
    // views::View methods:
    virtual void Layout();
    virtual gfx::Size GetPreferredSize();
    virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                      views::View* child);

    // views::Textfield::Controller methods:
    virtual void ContentsChanged(views::Textfield* sender,
                                 const string16& new_contents);
    virtual bool HandleKeystroke(views::Textfield* sender,
                                 const views::Textfield::Keystroke& keystroke);

    // views::ButtonListener methods:
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event);
    // views::Combobox::Listener methods:
    virtual void ItemChanged(views::Combobox* combo_box,
                             int prev_index,
                             int new_index);
   private:
    void InitTitle(views::GridLayout* layout);
    void InitAddressFields(views::GridLayout* layout);
    void InitCreditCardFields(views::GridLayout* layout);
    void InitLayoutGrid(views::GridLayout* layout);
    views::Label* CreateLeftAlignedLabel(int label_id);

    enum TextFields {
      TEXT_LABEL,
      TEXT_FIRST_NAME,
      TEXT_MIDDLE_NAME,
      TEXT_LAST_NAME,
      TEXT_EMAIL,
      TEXT_COMPANY_NAME,
      TEXT_ADDRESS_LINE_1,
      TEXT_ADDRESS_LINE_2,
      TEXT_ADDRESS_CITY,
      TEXT_ADDRESS_STATE,
      TEXT_ADDRESS_ZIP,
      TEXT_ADDRESS_COUNTRY,
      TEXT_PHONE_COUNTRY,
      TEXT_PHONE_AREA,
      TEXT_PHONE_PHONE,
      TEXT_FAX_COUNTRY,
      TEXT_FAX_AREA,
      TEXT_FAX_PHONE,
      TEXT_CC_NAME,
      TEXT_CC_NUMBER,
      TEXT_CC_EXPIRATION_MONTH,
      TEXT_CC_EXPIRATION_YEAR,
      TEXT_CC_EXPIRATION_CVC,
      // must be last
      MAX_TEXT_FIELD
    };
    views::Textfield* text_fields_[MAX_TEXT_FIELD];
    std::vector<EditableSetInfo>::iterator editable_fields_set_;
    views::ImageButton* expand_item_button_;
    views::Label* title_label_;
    views::Label* title_label_preview_;
    views::Button* delete_button_;
    AutoFillProfilesView* observer_;
    AddressComboBoxModel* billing_model_;
    AddressComboBoxModel* shipping_model_;
    views::Combobox* combo_box_billing_;
    views::Combobox* combo_box_shipping_;

    struct TextFieldToAutoFill {
      TextFields text_field;
      AutoFillFieldType type;
    };

    static TextFieldToAutoFill address_fields_[];
    static TextFieldToAutoFill credit_card_fields_[];

    static const int double_column_fill_view_set_id_ = 0;
    static const int double_column_leading_view_set_id_ = 1;
    static const int triple_column_fill_view_set_id_ = 2;
    static const int triple_column_leading_view_set_id_ = 3;
    static const int four_column_city_state_zip_set_id_ = 4;
    static const int four_column_ccnumber_expiration_cvc_ = 5;
    static const int three_column_header_ = 6;

    DISALLOW_COPY_AND_ASSIGN(EditableSetViewContents);
  };

  // Encapsulates ComboboxModel for address
  class AddressComboBoxModel : public ComboboxModel {
   public:
    explicit AddressComboBoxModel(bool is_billing);
    virtual ~AddressComboBoxModel() {}

    // Should be called only once. No other function should be called before it.
    // Does not own |address_labels|. To update the model text,
    // update label in one of the profiles and call LabelChanged()
    void set_address_labels(const std::vector<EditableSetInfo>* address_labels);

    // When you add a CB view that relies on this model, call this function
    // so the CB can be notified if strings change. Can be called multiple
    // times if several combo boxes relying on the model.
    // Model does not own |combo_box|.
    void UsedWithComboBox(views::Combobox *combo_box);

    // Call this function if one of the labels has changed
    void LabelChanged();

    // Gets index of the string in the model or -1 if not found.
    int GetIndex(const string16 &s);

    // ComboboxModel methods, public as they used from EditableSetViewContents
    virtual int GetItemCount();
    virtual std::wstring GetItemAt(int index);

   private:
    std::list<views::Combobox *> combo_boxes_;
    const std::vector<EditableSetInfo>* address_labels_;
    bool is_billing_;

    DISALLOW_COPY_AND_ASSIGN(AddressComboBoxModel);
  };

  // Sub-view  for scrolling credit cards and addresses
  class ScrollViewContents : public views::View,
                             public views::ButtonListener {
   public:
    ScrollViewContents(AutoFillProfilesView* observer,
                       std::vector<EditableSetInfo>* profiles,
                       std::vector<EditableSetInfo>* credit_cards);
    virtual ~ScrollViewContents() {}

   protected:
    // views::View methods:
    virtual int GetLineScrollIncrement(views::ScrollView* scroll_view,
                                       bool is_horizontal, bool is_positive);
    virtual void Layout();
    virtual gfx::Size GetPreferredSize();
    virtual void ViewHierarchyChanged(bool is_add, views::View* parent,
                                      views::View* child);
    // views::ButtonListener methods:
    virtual void ButtonPressed(views::Button* sender,
                               const views::Event& event);
   private:
    void Init();

    std::vector<EditableSetInfo>* profiles_;
    std::vector<EditableSetInfo>* credit_cards_;
    views::Button* add_address_;
    views::Button* add_credit_card_;
    AutoFillProfilesView* observer_;

    AddressComboBoxModel billing_model_;
    AddressComboBoxModel shipping_model_;

    static int line_height_;

    DISALLOW_COPY_AND_ASSIGN(ScrollViewContents);
  };

  class AutoFillScrollView : public views::View {
   public:
    AutoFillScrollView(AutoFillProfilesView* observer,
                       std::vector<EditableSetInfo>* profiles,
                       std::vector<EditableSetInfo>* credit_cards);
    virtual ~AutoFillScrollView() {}

    // Rebuilds the view by deleting and re-creating sub-views
    void RebuildView();

   protected:
    // views::View overrides:
    virtual void Layout();

   private:
    // The scroll view that contains list of the profiles.
    views::ScrollView* scroll_view_;
    ScrollViewContents* scroll_contents_view_;

    std::vector<EditableSetInfo>* profiles_;
    std::vector<EditableSetInfo>* credit_cards_;
    AutoFillProfilesView* observer_;

    DISALLOW_COPY_AND_ASSIGN(AutoFillScrollView);
  };

  AutoFillDialogObserver* observer_;
  std::vector<EditableSetInfo> profiles_set_;
  std::vector<EditableSetInfo> credit_card_set_;

  views::Button* save_changes_;
  AutoFillScrollView* scroll_view_;

  static AutoFillProfilesView* instance_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillProfilesView);
};

#endif  // CHROME_BROWSER_VIEWS_AUTOFILL_PROFILES_VIEW_WIN_H_

