// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/save_password_refusal_combobox_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/credentials_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_items_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/styled_label_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"


// Helpers --------------------------------------------------------------------

namespace {

const int kDesiredBubbleWidth = 370;

enum ColumnSetType {
  // | | (FILL, FILL) | |
  // Used for the bubble's header, the credentials list, and for simple
  // messages like "No passwords".
  SINGLE_VIEW_COLUMN_SET = 0,

  // | | (TRAILING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should nest at the
  // bottom-right corner.
  DOUBLE_BUTTON_COLUMN_SET = 1,

  // | | (LEADING, CENTER) | | (TRAILING, CENTER) | |
  // Used for buttons at the bottom of the bubble which should occupy
  // the corners.
  LINK_BUTTON_COLUMN_SET = 2,

  // | | (TRAILING, CENTER) | |
  // Used when there is only one button which should next at the bottom-right
  // corner.
  SINGLE_BUTTON_COLUMN_SET = 3,
};

enum TextRowType { ROW_SINGLE, ROW_MULTILINE };

// Construct an appropriate ColumnSet for the given |type|, and add it
// to |layout|.
void BuildColumnSet(views::GridLayout* layout, ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  int full_width = kDesiredBubbleWidth - (2 * views::kPanelHorizMargin);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::FIXED,
                            full_width,
                            0);
      break;

    case DOUBLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case LINK_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::LEADING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
    case SINGLE_BUTTON_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
  }
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
}

// Given |layout| and |color| adds border with |color| using
// SINGLE_VIEW_COLUMN_SET.
void AddBorderRow(views::GridLayout* layout, SkColor color) {
  layout->StartRowWithPadding(0, SINGLE_VIEW_COLUMN_SET, 0,
                              views::kRelatedControlVerticalSpacing);
  views::Separator* border = new views::Separator(views::Separator::HORIZONTAL);
  border->SetColor(color);
  layout->AddView(border);
}

// Given a layout and a model, add an appropriate title using a
// SINGLE_VIEW_COLUMN_SET, followed by a spacer row.
void AddTitleRow(views::GridLayout* layout, ManagePasswordsBubbleModel* model) {
  views::Label* title_label = new views::Label(model->title());
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetMultiLine(true);
  title_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::MediumFont));

  // Add the title to the layout with appropriate padding.
  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);
}

// Given a |layout| and |text|, adds a text row with small font using a
// SINGLE_VIEW_COLUMN_SET.
void AddTextRow(views::GridLayout* layout,
                const base::string16& text,
                TextRowType row_type) {
  views::Label* text_label = new views::Label(text);
  text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_label->SetMultiLine(row_type == ROW_MULTILINE);
  text_label->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(text_label);
}

// Returns the string representing the reported URL. This string will be shown
// to the user and reported through Feedback.
std::string PresentURL(const GURL& url) {
  return url.GetAsReferrer().spec();
}

}  // namespace


// Globals --------------------------------------------------------------------

namespace chrome {

void ShowManagePasswordsBubble(content::WebContents* web_contents) {
  if (ManagePasswordsBubbleView::IsShowing()) {
    // The bubble is currently shown for some other tab. We should close it now
    // and open for |web_contents|.
    ManagePasswordsBubbleView::CloseBubble();
  }
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents);
  ManagePasswordsBubbleView::ShowBubble(
      web_contents,
      password_manager::ui::IsAutomaticDisplayState(controller->state())
          ? ManagePasswordsBubbleView::AUTOMATIC
          : ManagePasswordsBubbleView::USER_ACTION);
}

void CloseManagePasswordsBubble(content::WebContents* web_contents) {
  if (!ManagePasswordsBubbleView::IsShowing())
    return;
  content::WebContents* bubble_web_contents =
      ManagePasswordsBubbleView::manage_password_bubble()->web_contents();
  if (web_contents == bubble_web_contents)
    ManagePasswordsBubbleView::CloseBubble();
}

}  // namespace chrome


// ManagePasswordsBubbleView::AccountChooserView ------------------------------

// A view offering the user the ability to choose credentials for
// authentication. Contains a list of CredentialsItemView, along with a
// "Cancel" button.
class ManagePasswordsBubbleView::AccountChooserView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit AccountChooserView(ManagePasswordsBubbleView* parent);
  ~AccountChooserView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;
  views::LabelButton* cancel_button_;
};

ManagePasswordsBubbleView::AccountChooserView::AccountChooserView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  cancel_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);
  cancel_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  const auto& pending_credentials = parent_->model()->pending_credentials();
  for (autofill::PasswordForm* form : pending_credentials) {
    CredentialsItemView* credential_view = new CredentialsItemView(this, *form);
    // Add the title to the layout with appropriate padding.
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(credential_view);
  }

  // Button row.
  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(cancel_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(cancel_button_);
}

ManagePasswordsBubbleView::AccountChooserView::~AccountChooserView() {
}

void ManagePasswordsBubbleView::AccountChooserView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender != cancel_button_) {
    // ManagePasswordsBubbleModel should care about calling a callback in case
    // the bubble is dismissed by any other means.
    CredentialsItemView* view = static_cast<CredentialsItemView*>(sender);
    parent_->model()->OnChooseCredentials(view->form());
  } else {
    parent_->model()->OnNopeClicked();
  }
  parent_->Close();
}

// ManagePasswordsBubbleView::AskUserToSubmitURLView -------------------------

// Asks users if they want to report the URL when the password manager failed
// to detect the form. View has following structure:
// We detected that Chrome password manager failed to handle this URL.
// Do you want to send this URL to Google to improve Chrome?
// -------------------------------------------------------------
// https://strangesite.com/
// -------------------------------------------------------------
//                                  [Send URL]            [Nope]
class ManagePasswordsBubbleView::AskUserToSubmitURLView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit AskUserToSubmitURLView(ManagePasswordsBubbleView* parent);
  ~AskUserToSubmitURLView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::LabelButton* send_button_;
  views::LabelButton* no_button_;
};

ManagePasswordsBubbleView::AskUserToSubmitURLView::AskUserToSubmitURLView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  GURL origin = parent->model()->origin();
  DCHECK(origin.is_valid() && !origin.is_empty() && origin.has_host());
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  send_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
  send_button_->SetStyle(views::Button::STYLE_BUTTON);
  no_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SEND_URL_BUTTON));
  no_button_->SetStyle(views::Button::STYLE_BUTTON);

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());
  // Confirmation text.
  AddTextRow(layout, l10n_util::GetStringUTF16(
                         IDS_MANAGE_PASSWORDS_ASK_TO_SUBMIT_URL_TEXT),
             ROW_MULTILINE);
  // Border row.
  SkColor color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_EnabledMenuButtonBorderColor);
  AddBorderRow(layout, color);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  // URL row.
  AddTextRow(layout, base::UTF8ToUTF16(PresentURL(parent->model()->origin())),
             ROW_SINGLE);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  // Border row.
  AddBorderRow(layout, color);
  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(0, DOUBLE_BUTTON_COLUMN_SET, 0,
                              views::kRelatedControlVerticalSpacing);
  layout->AddView(no_button_);
  layout->AddView(send_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(no_button_);
}

ManagePasswordsBubbleView::AskUserToSubmitURLView::~AskUserToSubmitURLView() {
}

void ManagePasswordsBubbleView::AskUserToSubmitURLView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == send_button_ || sender == no_button_);
  // TODO(melandory): Add actions on button clicks: url reporting, pref saving.
  if (sender == send_button_)
    parent_->model()->OnCollectURLClicked(
        PresentURL(parent_->model()->origin()));
  else if (sender == no_button_)
    parent_->model()->OnDoNotCollectURLClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::PendingView -------------------------------------

// A view offering the user the ability to save credentials. Contains a
// single ManagePasswordItemsView, along with a "Save Passwords" button
// and a rejection combobox.
class ManagePasswordsBubbleView::PendingView : public views::View,
                                               public views::ButtonListener,
                                               public views::ComboboxListener {
 public:
  explicit PendingView(ManagePasswordsBubbleView* parent);
  ~PendingView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Handles the event when the user changes an index of a combobox.
  void OnPerformAction(views::Combobox* source) override;

  ManagePasswordsBubbleView* parent_;

  views::BlueButton* save_button_;

  // The combobox doesn't take ownership of its model. If we created a
  // combobox we need to ensure that we delete the model here, and because the
  // combobox uses the model in it's destructor, we need to make sure we
  // delete the model _after_ the combobox itself is deleted.
  scoped_ptr<SavePasswordRefusalComboboxModel> combobox_model_;
  scoped_ptr<views::Combobox> refuse_combobox_;
};

ManagePasswordsBubbleView::PendingView::PendingView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  std::vector<const autofill::PasswordForm*> credentials(
      1, &parent->model()->pending_password());
  // Create the pending credential item, save button and refusal combobox.
  ManagePasswordItemsView* item =
      new ManagePasswordItemsView(parent_->model(), credentials);
  save_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));
  save_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  combobox_model_.reset(new SavePasswordRefusalComboboxModel());
  refuse_combobox_.reset(new views::Combobox(combobox_model_.get()));
  refuse_combobox_->set_listener(this);
  refuse_combobox_->SetStyle(views::Combobox::STYLE_ACTION);
  // TODO(mkwst): Need a mechanism to pipe a font list down into a combobox.

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // Credential row.
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(item);

  // Button row.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(save_button_);
  layout->AddView(refuse_combobox_.get());

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(save_button_);
}

ManagePasswordsBubbleView::PendingView::~PendingView() {
}

void ManagePasswordsBubbleView::PendingView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == save_button_);
  parent_->model()->OnSaveClicked();
  if (parent_->model()->state() ==
      password_manager::ui::SETUP_OS_PASSWORD_BUBBLE_STATE) {
    parent_->Refresh();
  } else {
    parent_->Close();
  }
}

void ManagePasswordsBubbleView::PendingView::OnPerformAction(
    views::Combobox* source) {
  DCHECK_EQ(source, refuse_combobox_);
  switch (refuse_combobox_->selected_index()) {
    case SavePasswordRefusalComboboxModel::INDEX_NOPE:
      parent_->model()->OnNopeClicked();
      parent_->Close();
      break;
    case SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE:
      parent_->NotifyNeverForThisSiteClicked();
      break;
  }
}

// ManagePasswordsBubbleView::SetupOSPasswordView -----------------------------

// A view giving the user a hint to set up an OS password. Contains two
// wide buttons leading to the Help Center and dismissing the bubble forever
// and a "Cancel" button.
class ManagePasswordsBubbleView::SetupOSPasswordView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit SetupOSPasswordView(ManagePasswordsBubbleView* parent);

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::LabelButton* read_more_button_;
  views::LabelButton* hide_button_;
  views::LabelButton* cancel_button_;
};

ManagePasswordsBubbleView::SetupOSPasswordView::SetupOSPasswordView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  read_more_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SETUP_OS_LINK_BUTTON));
  read_more_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont));
  hide_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_DISMISS_OS_BUBBLE_BUTTON));
  hide_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::BaseFont));
  cancel_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  cancel_button_->SetStyle(views::Button::STYLE_BUTTON);
  cancel_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(read_more_button_);
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(hide_button_);

  // Button row.
  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(cancel_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(cancel_button_);
}

void ManagePasswordsBubbleView::SetupOSPasswordView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == read_more_button_) {
    parent_->model()->OnShowOSPasswordHelpArticle();
  } else if (sender == hide_button_) {
    parent_->model()->OnHideOSPasswordBubble(true);
  } else {
    DCHECK(sender == cancel_button_);
    parent_->model()->OnHideOSPasswordBubble(false);
  }
  parent_->Close();
}

// ManagePasswordsBubbleView::ConfirmNeverView --------------------------------

// A view offering the user the ability to undo her decision to never save
// passwords for a particular site.
class ManagePasswordsBubbleView::ConfirmNeverView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit ConfirmNeverView(ManagePasswordsBubbleView* parent);
  ~ConfirmNeverView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::LabelButton* confirm_button_;
  views::LabelButton* undo_button_;
};

ManagePasswordsBubbleView::ConfirmNeverView::ConfirmNeverView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Title row.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // Confirmation text.
  views::Label* confirmation = new views::Label(l10n_util::GetStringUTF16(
      IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_TEXT));
  confirmation->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  confirmation->SetMultiLine(true);
  confirmation->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Confirm and undo buttons.
  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);

  confirm_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(
          IDS_MANAGE_PASSWORDS_BLACKLIST_CONFIRMATION_BUTTON));
  confirm_button_->SetStyle(views::Button::STYLE_BUTTON);
  confirm_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  layout->AddView(confirm_button_);

  undo_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_CANCEL));
  undo_button_->SetStyle(views::Button::STYLE_BUTTON);
  undo_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->AddView(undo_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(confirm_button_);
}

ManagePasswordsBubbleView::ConfirmNeverView::~ConfirmNeverView() {
}

void ManagePasswordsBubbleView::ConfirmNeverView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == confirm_button_ || sender == undo_button_);
  if (sender == confirm_button_)
    parent_->NotifyConfirmedNeverForThisSite();
  else
    parent_->NotifyUndoNeverForThisSite();
}

// ManagePasswordsBubbleView::ManageView --------------------------------------

// A view offering the user a list of her currently saved credentials
// for the current page, along with a "Manage passwords" link and a
// "Done" button.
class ManagePasswordsBubbleView::ManageView : public views::View,
                                              public views::ButtonListener,
                                              public views::LinkListener {
 public:
  explicit ManageView(ManagePasswordsBubbleView* parent);
  ~ManageView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::Link* manage_link_;
  views::LabelButton* done_button_;
};

ManagePasswordsBubbleView::ManageView::ManageView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // If we have a list of passwords to store for the current site, display
  // them to the user for management. Otherwise, render a "No passwords for
  // this site" message.
  if (!parent_->model()->best_matches().empty()) {
      std::vector<const autofill::PasswordForm*> password_forms;
      for (auto password_form : parent_->model()->best_matches()) {
        password_forms.push_back(password_form.second);
      }
      ManagePasswordItemsView* item = new ManagePasswordItemsView(
          parent_->model(), password_forms);
      layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
      layout->AddView(item);
  } else {
    views::Label* empty_label = new views::Label(
        l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
    empty_label->SetMultiLine(true);
    empty_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    empty_label->SetFontList(
        ui::ResourceBundle::GetSharedInstance().GetFontList(
            ui::ResourceBundle::SmallFont));

    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(empty_label);
    layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  }

  // Then add the "manage passwords" link and "Done" button.
  manage_link_ = new views::Link(parent_->model()->manage_link());
  manage_link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  manage_link_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  manage_link_->SetUnderline(false);
  manage_link_->set_listener(this);

  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, LINK_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, LINK_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(manage_link_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(done_button_);
}

ManagePasswordsBubbleView::ManageView::~ManageView() {
}

void ManagePasswordsBubbleView::ManageView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  DCHECK(sender == done_button_);
  parent_->model()->OnDoneClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::ManageView::LinkClicked(views::Link* source,
                                                        int event_flags) {
  DCHECK_EQ(source, manage_link_);
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::BlacklistedView ---------------------------------

// A view offering the user the ability to re-enable the password manager for
// a specific site after she's decided to "never save passwords".
class ManagePasswordsBubbleView::BlacklistedView
    : public views::View,
      public views::ButtonListener {
 public:
  explicit BlacklistedView(ManagePasswordsBubbleView* parent);
  ~BlacklistedView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ManagePasswordsBubbleView* parent_;

  views::BlueButton* unblacklist_button_;
  views::LabelButton* done_button_;
};

ManagePasswordsBubbleView::BlacklistedView::BlacklistedView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  // Add the title.
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  // Add the "Hey! You blacklisted this site!" text.
  views::Label* blacklisted = new views::Label(
      l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_BLACKLISTED));
  blacklisted->SetMultiLine(true);
  blacklisted->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));
  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(blacklisted);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Then add the "enable password manager" and "Done" buttons.
  unblacklist_button_ = new views::BlueButton(
      this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_UNBLACKLIST_BUTTON));
  unblacklist_button_->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  done_button_ =
      new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
  done_button_->SetStyle(views::Button::STYLE_BUTTON);
  done_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, DOUBLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, DOUBLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(unblacklist_button_);
  layout->AddView(done_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(unblacklist_button_);
}

ManagePasswordsBubbleView::BlacklistedView::~BlacklistedView() {
}

void ManagePasswordsBubbleView::BlacklistedView::ButtonPressed(
    views::Button* sender,
    const ui::Event& event) {
  if (sender == done_button_)
    parent_->model()->OnDoneClicked();
  else if (sender == unblacklist_button_)
    parent_->model()->OnUnblacklistClicked();
  else
    NOTREACHED();
  parent_->Close();
}

// ManagePasswordsBubbleView::SaveConfirmationView ----------------------------

// A view confirming to the user that a password was saved and offering a link
// to the Google account manager.
class ManagePasswordsBubbleView::SaveConfirmationView
    : public views::View,
      public views::ButtonListener,
      public views::StyledLabelListener {
 public:
  explicit SaveConfirmationView(ManagePasswordsBubbleView* parent);
  ~SaveConfirmationView() override;

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::StyledLabelListener implementation
  void StyledLabelLinkClicked(const gfx::Range& range,
                              int event_flags) override;

  ManagePasswordsBubbleView* parent_;

  views::LabelButton* ok_button_;
};

ManagePasswordsBubbleView::SaveConfirmationView::SaveConfirmationView(
    ManagePasswordsBubbleView* parent)
    : parent_(parent) {
  views::GridLayout* layout = new views::GridLayout(this);
  layout->set_minimum_size(gfx::Size(kDesiredBubbleWidth, 0));
  SetLayoutManager(layout);

  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  AddTitleRow(layout, parent_->model());

  views::StyledLabel* confirmation =
      new views::StyledLabel(parent_->model()->save_confirmation_text(), this);
  confirmation->SetBaseFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontList(
          ui::ResourceBundle::SmallFont));
  confirmation->AddStyleRange(
      parent_->model()->save_confirmation_link_range(),
      views::StyledLabel::RangeStyleInfo::CreateForLink());

  layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
  layout->AddView(confirmation);

  ok_button_ = new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_OK));
  ok_button_->SetStyle(views::Button::STYLE_BUTTON);
  ok_button_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
      ui::ResourceBundle::SmallFont));

  BuildColumnSet(layout, SINGLE_BUTTON_COLUMN_SET);
  layout->StartRowWithPadding(
      0, SINGLE_BUTTON_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
  layout->AddView(ok_button_);

  // Extra padding for visual awesomeness.
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  parent_->set_initially_focused_view(ok_button_);
}

ManagePasswordsBubbleView::SaveConfirmationView::~SaveConfirmationView() {
}

void ManagePasswordsBubbleView::SaveConfirmationView::StyledLabelLinkClicked(
    const gfx::Range& range, int event_flags) {
  DCHECK_EQ(range, parent_->model()->save_confirmation_link_range());
  parent_->model()->OnManageLinkClicked();
  parent_->Close();
}

void ManagePasswordsBubbleView::SaveConfirmationView::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(sender, ok_button_);
  parent_->model()->OnOKClicked();
  parent_->Close();
}

// ManagePasswordsBubbleView::WebContentMouseHandler --------------------------

// The class listens for WebContentsView events and notifies the bubble if the
// view was clicked on or received keystrokes.
class ManagePasswordsBubbleView::WebContentMouseHandler
    : public ui::EventHandler {
 public:
  explicit WebContentMouseHandler(ManagePasswordsBubbleView* bubble);
  ~WebContentMouseHandler() override;

  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  aura::Window* GetWebContentsWindow();

  ManagePasswordsBubbleView* bubble_;

  DISALLOW_COPY_AND_ASSIGN(WebContentMouseHandler);
};

ManagePasswordsBubbleView::WebContentMouseHandler::WebContentMouseHandler(
    ManagePasswordsBubbleView* bubble)
    : bubble_(bubble) {
  GetWebContentsWindow()->AddPreTargetHandler(this);
}

ManagePasswordsBubbleView::WebContentMouseHandler::~WebContentMouseHandler() {
  if (aura::Window* window = GetWebContentsWindow())
    window->RemovePreTargetHandler(this);
}

void ManagePasswordsBubbleView::WebContentMouseHandler::OnKeyEvent(
    ui::KeyEvent* event) {
  content::WebContents* web_contents = bubble_->model()->web_contents();
  content::RenderViewHost* rvh = web_contents->GetRenderViewHost();
  if (rvh->IsFocusedElementEditable() &&
      event->type() == ui::ET_KEY_PRESSED)
    bubble_->Close();
}

void ManagePasswordsBubbleView::WebContentMouseHandler::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    bubble_->Close();
}

aura::Window*
ManagePasswordsBubbleView::WebContentMouseHandler::GetWebContentsWindow() {
  content::WebContents* web_contents = bubble_->model()->web_contents();
  return web_contents ? web_contents->GetNativeView() : NULL;
}

// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           DisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());

  if (IsShowing())
    return;

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  ManagePasswordsIconView* anchor_view =
      is_fullscreen
          ? NULL
          : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, reason);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }
  if (reason == AUTOMATIC)
    manage_passwords_bubble_->GetWidget()->ShowInactive();
  else
    manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble() {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->Close();
}

// static
void ManagePasswordsBubbleView::ActivateBubble() {
  if (!IsShowing())
    return;
  manage_passwords_bubble_->GetWidget()->Activate();
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

content::WebContents* ManagePasswordsBubbleView::web_contents() const {
  return model()->web_contents();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    ManagePasswordsIconView* anchor_view,
    DisplayReason reason)
    : ManagePasswordsBubble(web_contents, reason),
      BubbleDelegateView(anchor_view,
                         anchor_view ? views::BubbleBorder::TOP_RIGHT
                                     : views::BubbleBorder::NONE),
      anchor_view_(anchor_view),
      initially_focused_view_(NULL) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  if (anchor_view)
    anchor_view->SetActive(true);
  mouse_handler_.reset(new WebContentMouseHandler(this));

  // Add observers to close the bubble if the fullscreen state changes.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_FULLSCREEN_CHANGED,
      content::Source<FullscreenController>(browser->fullscreen_controller()));
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (anchor_view_)
    anchor_view_->SetActive(false);
}

void ManagePasswordsBubbleView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  // The bubble's padding from the screen edge, used in fullscreen.
  const int kFullscreenPaddingEnd = 20;
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}

void ManagePasswordsBubbleView::Close() {
  mouse_handler_.reset();
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::Refresh() {
  RemoveAllChildViews(true);
  initially_focused_view_ = NULL;
  bool need_resize = false;
  if (password_manager::ui::IsPendingState(model()->state())) {
    if (model()->never_save_passwords())
      AddChildView(new ConfirmNeverView(this));
    else
      AddChildView(new PendingView(this));
  } else if (IsAskSubmitURLState(model()->state())) {
    AddChildView(new AskUserToSubmitURLView(this));
  } else if (model()->state() == password_manager::ui::BLACKLIST_STATE) {
    AddChildView(new BlacklistedView(this));
  } else if (model()->state() == password_manager::ui::CONFIRMATION_STATE) {
    AddChildView(new SaveConfirmationView(this));
  } else if (password_manager::ui::IsCredentialsState(model()->state())) {
    AddChildView(new AccountChooserView(this));
  } else if (model()->state() ==
      password_manager::ui::SETUP_OS_PASSWORD_BUBBLE_STATE) {
    AddChildView(new SetupOSPasswordView(this));
    need_resize = true;
  } else {
    AddChildView(new ManageView(this));
  }
  GetLayoutManager()->Layout(this);
  if (need_resize)
    SizeToContents();
}

void ManagePasswordsBubbleView::NotifyNeverForThisSiteClicked() {
  if (model()->best_matches().empty()) {
    // Skip confirmation if there are no existing passwords for this site.
    NotifyConfirmedNeverForThisSite();
  } else {
    model()->OnConfirmationForNeverForThisSite();
    Refresh();
  }
}

void ManagePasswordsBubbleView::NotifyConfirmedNeverForThisSite() {
  model()->OnNeverForThisSiteClicked();
  Close();
}

void ManagePasswordsBubbleView::NotifyUndoNeverForThisSite() {
  model()->OnUndoNeverForThisSite();
  Refresh();
}

void ManagePasswordsBubbleView::Init() {
  views::FillLayout* layout = new views::FillLayout();
  SetLayoutManager(layout);

  Refresh();
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

views::View* ManagePasswordsBubbleView::GetInitiallyFocusedView() {
  return initially_focused_view_;
}

void ManagePasswordsBubbleView::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_FULLSCREEN_CHANGED);
  GetWidget()->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  CloseBubble();
}
