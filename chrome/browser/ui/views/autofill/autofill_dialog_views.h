// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_

#include <map>

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/window/dialog_delegate.h"

class ConstrainedWindowViews;

namespace content {
class KeyboardListener;
}

namespace views {
class Checkbox;
class Combobox;
class FocusManager;
class ImageButton;
class Label;
class MenuRunner;
class TextButton;
class Textfield;
class WebView;
}

namespace ui {
class ComboboxModel;
class KeyEvent;
}

namespace autofill {

struct DetailInput;

// Views toolkit implementation of the Autofill dialog that handles the
// imperative autocomplete API call.
class AutofillDialogViews : public AutofillDialogView,
                            public views::DialogDelegate,
                            public views::ButtonListener,
                            public views::TextfieldController,
                            public views::FocusChangeListener,
                            public views::LinkListener {
 public:
  explicit AutofillDialogViews(AutofillDialogController* controller);
  virtual ~AutofillDialogViews();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            DetailOutputMap* output) OVERRIDE;
  virtual bool UseBillingForShipping() OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController& ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;

  // views::DialogDelegate implementation:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void WindowClosing() OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual views::View* GetExtraView() OVERRIDE;
  virtual bool Cancel() OVERRIDE;
  virtual bool Accept() OVERRIDE;

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::TextfieldController implementation:
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) OVERRIDE;
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE;
  virtual bool HandleMouseEvent(views::Textfield* sender,
                                const ui::MouseEvent& key_event) OVERRIDE;

  // views::FocusChangeListener implementation.
  virtual void OnWillChangeFocus(views::View* focused_before,
                                 views::View* focused_now) OVERRIDE;
  virtual void OnDidChangeFocus(views::View* focused_before,
                                views::View* focused_now) OVERRIDE;

  // views::LinkListener implementation:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  // A class which holds a textfield and draws extra stuff on top, like credit
  // card icons or invalid content indications.
  class DecoratedTextfield : public views::View {
   public:
    DecoratedTextfield(const string16& default_value,
                       const string16& placeholder,
                       views::TextfieldController* controller);
    virtual ~DecoratedTextfield();

    // The wrapped text field.
    views::Textfield* textfield() { return textfield_; }

    // Sets whether to indicate the textfield has invalid content.
    void SetInvalid(bool invalid);
    bool invalid() const { return invalid_; }

    // views::View implementation.
    virtual std::string GetClassName() const OVERRIDE;
    virtual void PaintChildren(gfx::Canvas* canvas) OVERRIDE;
    virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

   private:
    views::Textfield* textfield_;
    bool invalid_;

    DISALLOW_COPY_AND_ASSIGN(DecoratedTextfield);
  };

  typedef std::map<const DetailInput*, DecoratedTextfield*> TextfieldMap;
  typedef std::map<const DetailInput*, views::Combobox*> ComboboxMap;

  // A view that packs a label on the left and some related controls
  // on the right.
  class SectionContainer : public views::View {
   public:
    SectionContainer(const string16& label,
                     views::View* controls,
                     views::Button* proxy_button);
    virtual ~SectionContainer();

    // Sets whether mouse events should be forwarded to |proxy_button_|.
    void SetForwardMouseEvents(bool forward);

    // views::View implementation.
    virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
    virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
    virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
    virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;

   private:
    // Converts |event| to one suitable for |proxy_button_|.
    static ui::MouseEvent ProxyEvent(const ui::MouseEvent& event);

    // Mouse events on |this| are sent to this button.
    views::Button* proxy_button_;  // Weak reference.

    // When true, mouse events will be forwarded to |proxy_button_|.
    bool forward_mouse_events_;

    DISALLOW_COPY_AND_ASSIGN(SectionContainer);
  };

  // A view that contains a suggestion (such as a known address) and a link to
  // edit the suggestion.
  class SuggestionView : public views::View {
   public:
    SuggestionView(const string16& edit_label,
                   views::LinkListener* edit_listener);
    virtual ~SuggestionView();

    // Sets the display text of the suggestion. TODO(estade): this needs to
    // support credit card icons as well.
    void SetSuggestionText(const string16& text);

   private:
    // The label that holds the suggestion description text.
    views::Label* label_;

    DISALLOW_COPY_AND_ASSIGN(SuggestionView);
  };

  // A convenience struct for holding pointers to views within each detail
  // section. None of the member pointers are owned.
  struct DetailsGroup {
    explicit DetailsGroup(DialogSection section);
    ~DetailsGroup();

    // The section this group is associated with.
    const DialogSection section;
    // The view that contains the entire section (label + input).
    SectionContainer* container;
    // The view that allows manual input.
    views::View* manual_input;
    // The textfields in |manual_input|, tracked by their DetailInput.
    TextfieldMap textfields;
    // The comboboxes in |manual_input|, tracked by their DetailInput.
    ComboboxMap comboboxes;
    // The view that holds the text of the suggested data. This will be
    // visible IFF |manual_input| is not visible.
    SuggestionView* suggested_info;
    // The view that allows selecting other data suggestions.
    views::ImageButton* suggested_button;
  };

  typedef std::map<DialogSection, DetailsGroup> DetailGroupMap;

  void InitChildViews();

  // Creates and returns a view that holds all detail sections.
  views::View* CreateDetailsContainer();

  // Creates and returns a view that holds the requesting host and intro text.
  views::View* CreateNotificationArea();

  // Creates and returns a view that holds a sign in page and related controls.
  views::View* CreateSignInContainer();

  // Creates and returns a view that holds the main controls of this dialog.
  views::View* CreateMainContainer();

  // Creates a detail section (Shipping, Email, etc.) with the given label,
  // inputs View, and suggestion model. Relevant pointers are stored in |group|.
  void CreateDetailsSection(DialogSection section);

  // Like CreateDetailsSection, but creates the combined billing/cc section,
  // which is somewhat more complicated than the others.
  void CreateBillingSection();

  // Creates the view that holds controls for inputing or selecting data for
  // a given section.
  views::View* CreateInputsContainer(DialogSection section);

  // Creates a grid of textfield views for the given section, and stores them
  // in the appropriate DetailsGroup. The top level View in the hierarchy is
  // returned.
  views::View* InitInputsView(DialogSection section);

  // Updates the visual state of the given group as per the model.
  void UpdateDetailsGroupState(const DetailsGroup& group);

  // Returns true if at least one of the details sections is in manual input
  // mode.
  bool AtLeastOneSectionIsEditing();

  // Gets a pointer to the DetailsGroup that's associated with the given section
  // of the dialog.
  DetailsGroup* GroupForSection(DialogSection section);

  // Checks all manual inputs in the form for validity. Decorates the invalid
  // ones and returns true if all were valid.
  bool ValidateForm();

  // When an input textfield is edited (its contents change) or activated
  // (clicked while focused), this function will inform the controller that it's
  // time to show a suggestion popup and possibly reset the validity state of
  // the input.
  void TextfieldEditedOrActivated(views::Textfield* textfield, bool was_edit);

  // The controller that drives this view. Weak pointer, always non-NULL.
  AutofillDialogController* const controller_;

  // True if the termination action was a submit.
  bool did_submit_;

  // The window that displays |contents_|. Weak pointer; may be NULL when the
  // dialog is closing.
  ConstrainedWindowViews* window_;

  // The top-level View for the dialog. Owned by the constrained window.
  views::View* contents_;

  // A DialogSection-keyed map of the DetailGroup structs.
  DetailGroupMap detail_groups_;

  // Somewhere to show notification messages about errors, warnings, or promos.
  views::Label* notification_label_;

  // The checkbox that controls whether to use the billing details for shipping
  // as well.
  views::Checkbox* use_billing_for_shipping_;

  // Runs the suggestion menu (triggered by each section's |suggested_button|.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // The link that initiates the sign in flow.
  views::Link* sign_in_link_;

  // View to host the signin dialog and related controls.
  views::View* sign_in_container_;

  // TextButton displayed during sign-in. Clicking cancels sign-in and returns
  // to the main flow.
  views::TextButton* cancel_sign_in_;

  // A WebView to that navigates to a Google sign-in page to allow the user to
  // sign-in.
  views::WebView* sign_in_webview_;

  // View to host everything that isn't related to sign-in.
  views::View* main_container_;

  // The "Extra view" is on the same row as the dialog buttons.
  views::View* button_strip_extra_view_;

  // This checkbox controls whether new details are saved to the Autofill
  // database. It lives in |extra_view_|.
  views::Checkbox* save_in_chrome_checkbox_;

  // The focus manager for |window_|.
  views::FocusManager* focus_manager_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
