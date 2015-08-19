// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_

#include <map>
#include <set>

#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/views/window/dialog_delegate.h"

namespace gfx {
class Image;
}

namespace views {
class BubbleBorder;
class Checkbox;
class Combobox;
class FocusManager;
class ImageView;
class Label;
class LabelButton;
class Link;
class MenuRunner;
class StyledLabel;
class WebView;
class Widget;
}  // namespace views

namespace ui {
class ComboboxModel;
class EventHandler;
class KeyEvent;
}

namespace autofill {

class AutofillDialogSignInDelegate;
class ExpandingTextfield;
class InfoBubble;

// Views toolkit implementation of the Autofill dialog that handles the
// imperative autocomplete API call.
class AutofillDialogViews : public AutofillDialogView,
                            public views::DialogDelegateView,
                            public views::WidgetObserver,
                            public views::TextfieldController,
                            public views::FocusChangeListener,
                            public views::ComboboxListener,
                            public views::MenuButtonListener {
 public:
  explicit AutofillDialogViews(AutofillDialogViewDelegate* delegate);
  ~AutofillDialogViews() override;

  // AutofillDialogView implementation:
  void Show() override;
  void Hide() override;
  void UpdatesStarted() override;
  void UpdatesFinished() override;
  void UpdateButtonStrip() override;
  void UpdateDetailArea() override;
  void UpdateForErrors() override;
  void UpdateNotificationArea() override;
  void UpdateSection(DialogSection section) override;
  void UpdateErrorBubble() override;
  void FillSection(DialogSection section,
                   ServerFieldType originating_type) override;
  void GetUserInput(DialogSection section, FieldValueMap* output) override;
  base::string16 GetCvc() override;
  bool SaveDetailsLocally() override;
  void ModelChanged() override;
  void ValidateSection(DialogSection section) override;

  // views::View implementation.
  gfx::Size GetPreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;

  // views::DialogDelegate implementation:
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  void DeleteDelegate() override;
  int GetDialogButtons() const override;
  int GetDefaultDialogButton() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  bool ShouldDefaultButtonBeBlue() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  views::View* CreateExtraView() override;
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetObserver implementation:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::TextfieldController implementation:
  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& key_event) override;

  // views::FocusChangeListener implementation.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ComboboxListener implementation:
  void OnPerformAction(views::Combobox* combobox) override;

  // views::MenuButtonListener implementation.
  void OnMenuButtonClicked(views::View* source,
                           const gfx::Point& point) override;

 protected:
  // Exposed for testing.
  views::View* GetNotificationAreaForTesting();
  views::View* GetScrollableAreaForTesting();

 private:
  friend class AutofillDialogViewTesterViews;

  // An area for notifications. Some notifications point at the account chooser.
  class NotificationArea : public views::View {
   public:
    explicit NotificationArea(AutofillDialogViewDelegate* delegate);
    ~NotificationArea() override;

    // Displays the given notifications.
    void SetNotifications(const std::vector<DialogNotification>& notifications);

    // views::View implementation.
    gfx::Size GetPreferredSize() const override;
    const char* GetClassName() const override;
    void PaintChildren(const ui::PaintContext& context) override;

    void set_arrow_centering_anchor(
        const base::WeakPtr<views::View>& arrow_centering_anchor) {
      arrow_centering_anchor_ = arrow_centering_anchor;
    }

   private:
    // Utility function for determining whether an arrow should be drawn
    // pointing at |arrow_centering_anchor_|.
    bool HasArrow();

    // A reference to the delegate/controller than owns this view.
    // Used to report when checkboxes change their values.
    AutofillDialogViewDelegate* delegate_;  // weak

    // If HasArrow() is true, the arrow should point at this.
    base::WeakPtr<views::View> arrow_centering_anchor_;

    std::vector<DialogNotification> notifications_;

    DISALLOW_COPY_AND_ASSIGN(NotificationArea);
  };

  typedef std::map<ServerFieldType, ExpandingTextfield*> TextfieldMap;
  typedef std::map<ServerFieldType, views::Combobox*> ComboboxMap;

  // A view that packs a label on the left and some related controls
  // on the right.
  class SectionContainer : public views::View,
                           public views::ViewTargeterDelegate {
   public:
    SectionContainer(const base::string16& label,
                     views::View* controls,
                     views::Button* proxy_button);
    ~SectionContainer() override;

    // Sets the visual appearance of the section to active (considered active
    // when showing the menu or hovered by the mouse cursor).
    void SetActive(bool active);

    // Sets whether mouse events should be forwarded to |proxy_button_|.
    void SetForwardMouseEvents(bool forward);

    // views::View implementation.
    const char* GetClassName() const override;
    void OnMouseMoved(const ui::MouseEvent& event) override;
    void OnMouseEntered(const ui::MouseEvent& event) override;
    void OnMouseExited(const ui::MouseEvent& event) override;
    bool OnMousePressed(const ui::MouseEvent& event) override;
    void OnMouseReleased(const ui::MouseEvent& event) override;
    void OnGestureEvent(ui::GestureEvent* event) override;

   private:
    // views::ViewTargeterDelegate:
    views::View* TargetForRect(views::View* root,
                               const gfx::Rect& rect) override;

    // Converts |event| to one suitable for |proxy_button_|.
    static ui::MouseEvent ProxyEvent(const ui::MouseEvent& event);

    // Returns true if the given event should be forwarded to |proxy_button_|.
    bool ShouldForwardEvent(const ui::LocatedEvent& event);

    // Mouse events on |this| are sent to this button.
    views::Button* proxy_button_;  // Weak reference.

    // When true, all mouse events will be forwarded to |proxy_button_|.
    bool forward_mouse_events_;

    DISALLOW_COPY_AND_ASSIGN(SectionContainer);
  };

  // A button to show address or billing suggestions.
  class SuggestedButton : public views::MenuButton {
   public:
    explicit SuggestedButton(views::MenuButtonListener* listener);
    ~SuggestedButton() override;

    // views::MenuButton implementation.
    gfx::Size GetPreferredSize() const override;
    const char* GetClassName() const override;
    void OnPaint(gfx::Canvas* canvas) override;

   private:
    // Returns the corred resource ID (i.e. IDR_*) for the current |state()|.
    int ResourceIDForState() const;

    DISALLOW_COPY_AND_ASSIGN(SuggestedButton);
  };

  // A view that runs a callback whenever its bounds change, and which can
  // optionally suppress layout.
  class DetailsContainerView : public views::View {
   public:
    explicit DetailsContainerView(const base::Closure& callback);
    ~DetailsContainerView() override;

    void set_ignore_layouts(bool ignore_layouts) {
      ignore_layouts_ = ignore_layouts;
    }

    // views::View implementation.
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
    void Layout() override;

   private:
    base::Closure bounds_changed_callback_;

    // The view ignores Layout() calls when this is true.
    bool ignore_layouts_;

    DISALLOW_COPY_AND_ASSIGN(DetailsContainerView);
  };

  // A view that contains a suggestion (such as a known address).
  class SuggestionView : public views::View {
   public:
    explicit SuggestionView(AutofillDialogViews* autofill_dialog);
    ~SuggestionView() override;

    void SetState(const SuggestionState& state);

    // views::View implementation.
    gfx::Size GetPreferredSize() const override;
    int GetHeightForWidth(int width) const override;
    void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

    ExpandingTextfield* textfield() { return textfield_; }

   private:
    // Returns whether there's room to display |state_.vertically_compact_text|
    // without resorting to an ellipsis for a pixel width of |available_width|.
    // Fills in |resulting_height| with the amount of space required to display
    // |vertically_compact_text| or |horizontally_compact_text| as the case may
    // be.
    bool CanUseVerticallyCompactText(int available_width,
                                     int* resulting_height) const;

    // Sets the display text of the suggestion.
    void SetLabelText(const base::string16& text);

    // Sets the icon which should be displayed ahead of the text.
    void SetIcon(const gfx::Image& image);

    // Shows an auxiliary textfield to the right of the suggestion icon and
    // text. This is currently only used to show a CVC field for the CC section.
    void SetTextfield(const base::string16& placeholder_text,
                      const gfx::Image& icon);

    // Calls SetLabelText() with the appropriate text based on current bounds.
    void UpdateLabelText();

    // The state of |this|.
    SuggestionState state_;

    // This caches preferred heights for given widths. The key is a preferred
    // width, the value is a cached result of CanUseVerticallyCompactText.
    mutable std::map<int, std::pair<bool, int> > calculated_heights_;

    // The label that holds the suggestion description text.
    views::Label* label_;
    // The second (and greater) line of text that describes the suggestion.
    views::Label* label_line_2_;
    // The icon that comes just before |label_|.
    views::ImageView* icon_;
    // The input set by ShowTextfield.
    ExpandingTextfield* textfield_;

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
    // The textfields in |manual_input|, tracked by their ServerFieldType.
    TextfieldMap textfields;
    // The comboboxes in |manual_input|, tracked by their ServerFieldType.
    ComboboxMap comboboxes;
    // The view that holds the text of the suggested data. This will be
    // visible IFF |manual_input| is not visible.
    SuggestionView* suggested_info;
    // The view that allows selecting other data suggestions.
    SuggestedButton* suggested_button;
  };

  typedef std::map<DialogSection, DetailsGroup> DetailGroupMap;

  // Returns the preferred size or minimum size (if |get_minimum_size| is true).
  gfx::Size CalculatePreferredSize(bool get_minimum_size) const;

  // Returns the minimum size of the sign in view for this dialog.
  gfx::Size GetMinimumSignInViewSize() const;

  // Returns the maximum size of the sign in view for this dialog.
  gfx::Size GetMaximumSignInViewSize() const;

  // Returns which section should currently be used for credit card info.
  DialogSection GetCreditCardSection() const;

  void InitChildViews();

  // Creates and returns a view that holds all detail sections.
  views::View* CreateDetailsContainer();

  // Creates and returns a view that holds the requesting host and intro text.
  views::View* CreateNotificationArea();

  // Creates and returns a view that holds the main controls of this dialog.
  views::View* CreateMainContainer();

  // Creates a detail section (Shipping, Email, etc.) with the given label,
  // inputs View, and suggestion model. Relevant pointers are stored in |group|.
  void CreateDetailsSection(DialogSection section);

  // Creates the view that holds controls for inputing or selecting data for
  // a given section.
  views::View* CreateInputsContainer(DialogSection section);

  // Creates a grid of inputs for the given section.
  void InitInputsView(DialogSection section);

  // Updates the given section to match the state provided by |delegate_|. If
  // |clobber_inputs| is true, the current state of the textfields will be
  // ignored, otherwise their contents will be preserved.
  void UpdateSectionImpl(DialogSection section, bool clobber_inputs);

  // Updates the visual state of the given group as per the model.
  void UpdateDetailsGroupState(const DetailsGroup& group);

  // Gets a pointer to the DetailsGroup that's associated with the given section
  // of the dialog.
  DetailsGroup* GroupForSection(DialogSection section);

  // Gets a pointer to the DetailsGroup that's associated with a given |view|.
  // Returns NULL if no DetailsGroup was found.
  DetailsGroup* GroupForView(views::View* view);

  // Erases all views in |group| from |validity_map_|.
  void EraseInvalidViewsInGroup(const DetailsGroup* group);

  // Explicitly focuses the initially focusable view.
  void FocusInitialView();

  // Sets the visual state for an input to be either valid or invalid. This
  // should work on Comboboxes or ExpandingTextfields. If |message| is empty,
  // the input is valid.
  template<class T>
  void SetValidityForInput(T* input, const base::string16& message);

  // Shows an error bubble pointing at |view| if |view| has a message in
  // |validity_map_|.
  void ShowErrorBubbleForViewIfNecessary(views::View* view);

  // Hides |error_bubble_| (if it exists).
  void HideErrorBubble();

  // Updates validity of the inputs in |section| with new |validity_messages|.
  // Fields are only updated with unsure messages if |overwrite_valid| is true.
  void MarkInputsInvalid(DialogSection section,
                         const ValidityMessages& validity_messages,
                         bool overwrite_invalid);

  // Checks all manual inputs in |group| for validity. Decorates the invalid
  // ones and returns true if all were valid.
  bool ValidateGroup(const DetailsGroup& group, ValidationType type);

  // Checks all manual inputs in the form for validity. Decorates the invalid
  // ones and returns true if all were valid.
  bool ValidateForm();

  // When an input is edited (its contents change) or activated (clicked while
  // focused), this function will inform the delegate to take the appropriate
  // action (textfields may show a suggestion popup, comboboxes may rebuild the
  // section inputs). May also reset the validity state of the input.
  void InputEditedOrActivated(ServerFieldType type,
                              const gfx::Rect& bounds,
                              bool was_edit);

  // Updates the views in the button strip.
  void UpdateButtonStripExtraView();

  // Call this when the size of anything in the contents might have changed.
  void ContentsPreferredSizeChanged();
  void DoContentsPreferredSizeChanged();

  // Gets the textfield view that is shown for the given |type| or NULL.
  ExpandingTextfield* TextfieldForType(ServerFieldType type);

  // Returns the associated ServerFieldType for |textfield|.
  ServerFieldType TypeForTextfield(const views::View* textfield);

  // Gets the combobox view that is shown for the given |type|, or NULL.
  views::Combobox* ComboboxForType(ServerFieldType type);

  // Returns the associated ServerFieldType for |combobox|.
  ServerFieldType TypeForCombobox(const views::Combobox* combobox) const;

  // Called when the details container changes in size or position.
  void DetailsContainerBoundsChanged();

  // Sets the icons in |section| according to the field values. For example,
  // sets the credit card and CVC icons according to the credit card number.
  void SetIconsForSection(DialogSection section);

  // Handles mouse presses on the non-client view.
  void NonClientMousePressed();

  // The delegate that drives this view. Weak pointer, always non-NULL.
  AutofillDialogViewDelegate* const delegate_;

  // The preferred size of the view, cached to avoid needless recomputation.
  mutable gfx::Size preferred_size_;

  // The current number of unmatched calls to UpdatesStarted.
  int updates_scope_;

  // True when there's been a call to ContentsPreferredSizeChanged() suppressed
  // due to an unmatched UpdatesStarted.
  bool needs_update_;

  // The window that displays the dialog contents. Weak pointer; may be NULL
  // when the dialog is closing.
  views::Widget* window_;

  // A DialogSection-keyed map of the DetailGroup structs.
  DetailGroupMap detail_groups_;

  // Somewhere to show notification messages about errors, warnings, or promos.
  NotificationArea* notification_area_;

  // Runs the suggestion menu (triggered by each section's |suggested_button|).
  scoped_ptr<views::MenuRunner> menu_runner_;

  // View that wraps |details_container_| and makes it scroll vertically.
  views::ScrollView* scrollable_area_;

  // View to host details sections.
  DetailsContainerView* details_container_;

  // The "Extra view" is on the same row as the dialog buttons.
  views::View* button_strip_extra_view_;

  // This checkbox controls whether new details are saved to the Autofill
  // database. It lives in |extra_view_|.
  views::Checkbox* save_in_chrome_checkbox_;

  // Holds the above checkbox and an associated tooltip icon.
  views::View* save_in_chrome_checkbox_container_;

  // View that aren't in the hierarchy but are owned by |this|. Currently
  // just holds the (hidden) country comboboxes.
  ScopedVector<views::View> other_owned_views_;

  // The focus manager for |window_|.
  views::FocusManager* focus_manager_;

  // The object that manages the error bubble widget.
  InfoBubble* error_bubble_;  // Weak; owns itself.

  // Map from input view (textfield or combobox) to error string.
  std::map<views::View*, base::string16> validity_map_;

  ScopedObserver<views::Widget, AutofillDialogViews> observer_;

  // Used to tell the delegate when focus moves to hide the Autofill popup.
  scoped_ptr<ui::EventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEWS_H_
