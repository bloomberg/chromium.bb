// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include <utility>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace autofill {

namespace {

// Returns a label that describes a details section.
views::Label* CreateDetailsSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  label->set_border(views::Border::CreateEmptyBorder(4, 0, 0, 0));
  return label;
}

// Creates a view that packs a label on the left and some related controls
// on the right.
views::View* CreateSectionContainer(const string16& label,
                                    views::View* controls) {
  views::View* container = new views::View();
  views::GridLayout* layout = new views::GridLayout(container);
  container->SetLayoutManager(layout);

  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  // TODO(estade): pull out these constants, and figure out better values
  // for them.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        180,
                        0);
  column_set->AddPaddingColumn(0, 15);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        300,
                        0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(CreateDetailsSectionLabel(label));
  layout->AddView(controls);
  return container;
}

}  // namespace

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      did_submit_(false),
      window_(NULL),
      contents_(NULL) {
  DCHECK(controller);
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. The
  // WebContentsModalDialog will take care of deleting itself after calling
  // DeleteDelegate().
  window_ = new ConstrainedWindowViews(controller_->web_contents(), this);
  window_->GetFocusManager()->AddFocusChangeListener(this);
}

void AutofillDialogViews::UpdateSection(DialogSection section) {
  const DetailInputs& updated_inputs =
      controller_->RequestedFieldsForSection(section);
  DetailsGroup* group = GroupForSection(section);

  for (DetailInputs::const_iterator iter = updated_inputs.begin();
       iter != updated_inputs.end(); ++iter) {
    TextfieldMap::iterator input = group->textfields.find(&(*iter));
    if (input == group->textfields.end())
      continue;

    input->second->SetText(iter->autofilled_value);
  }
}

int AutofillDialogViews::GetSuggestionSelection(DialogSection section) {
  return GroupForSection(section)->suggested_input->selected_index();
}

void AutofillDialogViews::GetUserInput(DialogSection section,
                                       DetailOutputMap* output) {
  DetailsGroup* group = GroupForSection(section);
  for (TextfieldMap::iterator it = group->textfields.begin();
       it != group->textfields.end(); ++it) {
    output->insert(std::make_pair(it->first, it->second->text()));
  }
  for (ComboboxMap::iterator it = group->comboboxes.begin();
       it != group->comboboxes.end(); ++it) {
    views::Combobox* combobox = it->second;
    output->insert(std::make_pair(
        it->first,
        combobox->model()->GetItemAt(combobox->selected_index())));
  }
}

bool AutofillDialogViews::UseBillingForShipping() {
  return use_billing_for_shipping_->checked();
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::WindowClosing() {
  window_->GetFocusManager()->RemoveFocusChangeListener(this);
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed(did_submit_ ? ACTION_SUBMIT : ACTION_ABORT);
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonEnabled() : true;
}

bool AutofillDialogViews::Cancel() {
  return true;
}

bool AutofillDialogViews::Accept() {
  did_submit_ = true;
  return true;
}

void AutofillDialogViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(sender, use_billing_for_shipping_);
  shipping_.container->SetVisible(!use_billing_for_shipping_->checked());
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void AutofillDialogViews::OnSelectedIndexChanged(views::Combobox* combobox) {
  DetailsGroup* group =
      combobox == email_.suggested_input ? &email_ :
      combobox == cc_.suggested_input ? &cc_ :
      combobox == billing_.suggested_input ? &billing_ :
      combobox == shipping_.suggested_input ? &shipping_ : NULL;
  DCHECK(group);
  UpdateDetailsGroupState(*group);
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void AutofillDialogViews::ContentsChanged(views::Textfield* sender,
                                          const string16& new_contents) {
  // TODO(estade): work for not billing.
  for (TextfieldMap::iterator iter = billing_.textfields.begin();
       iter != billing_.textfields.end();
       ++iter) {
    if (iter->second == sender) {
      controller_->UserEditedInput(iter->first,
                                   GetWidget()->GetNativeView(),
                                   sender->GetBoundsInScreen(),
                                   new_contents);
      break;
    }
  }
}

bool AutofillDialogViews::HandleKeyEvent(views::Textfield* sender,
                                         const ui::KeyEvent& key_event) {
  // TODO(estade): implement.
  return false;
}

void AutofillDialogViews::OnWillChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {
  controller_->FocusMoved();
}

void AutofillDialogViews::OnDidChangeFocus(
    views::View* focused_before,
    views::View* focused_now) {}

void AutofillDialogViews::InitChildViews() {
  contents_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_set = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  string16 security_warning(controller_->SecurityWarning());
  if (!security_warning.empty()) {
    layout->StartRow(0, single_column_set);

    views::Label* label = new views::Label(security_warning);
    label->SetAutoColorReadabilityEnabled(false);
    label->SetEnabledColor(SK_ColorRED);
    label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    label->set_border(views::Border::CreateEmptyBorder(0, 0, 10, 0));

    // TODO(dbeam): make this wrap when long. Just label->SetMultiline(true);
    // didn't seem to be enough.

    layout->AddView(label);
  }

  layout->StartRow(0, single_column_set);
  layout->AddView(CreateIntroContainer());

  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(CreateDetailsContainer());

  // Separator.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(new views::Separator());

  // Wallet checkbox.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(new views::Checkbox(controller_->WalletOptionText()));
}

views::View* AutofillDialogViews::CreateIntroContainer() {
  views::View* view = new views::View();
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));

  std::pair<string16, string16> intro_text_parts =
      controller_->GetIntroTextParts();

  view->AddChildView(new views::Label(intro_text_parts.first));

  views::Label* site_label = new views::Label(controller_->SiteLabel());
  site_label->SetFont(site_label->font().DeriveFont(0, gfx::Font::BOLD));
  view->AddChildView(site_label);

  view->AddChildView(new views::Label(intro_text_parts.second));

  return view;
}

views::View* AutofillDialogViews::CreateDetailsContainer() {
  views::View* view = new views::View();
  // A box layout is used because it respects widget visibility.
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));

  // Email.
  CreateDetailsSection(SECTION_EMAIL);
  view->AddChildView(email_.container);
  // Billing.
  CreateBillingSection();
  view->AddChildView(billing_.container);
  // Shipping.
  CreateDetailsSection(SECTION_SHIPPING);
  view->AddChildView(shipping_.container);
  shipping_.container->SetVisible(!use_billing_for_shipping_->checked());

  return view;
}

void AutofillDialogViews::CreateDetailsSection(DialogSection section) {
  DCHECK_NE(SECTION_CC, section);
  DCHECK_NE(SECTION_BILLING, section);

  // Inputs container (manual inputs + combobox).
  views::View* inputs_container = CreateInputsContainer(section);
  // Container (holds label + inputs).
  views::View* container = CreateSectionContainer(
      controller_->LabelForSection(section), inputs_container);

  DetailsGroup* group = GroupForSection(section);
  group->container = container;
}

void AutofillDialogViews::CreateBillingSection() {
  views::View* billing = new views::View();
  billing->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0,
      views::kRelatedControlVerticalSpacing));

  static const DialogSection sections[] = { SECTION_CC, SECTION_BILLING };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(sections); ++i) {
    // Inputs container (manual inputs + combobox).
    views::View* inputs_container = CreateInputsContainer(sections[i]);
    billing->AddChildView(inputs_container);
  }

  use_billing_for_shipping_ =
      new views::Checkbox(controller_->UseBillingForShippingText());
  use_billing_for_shipping_->SetChecked(true);
  use_billing_for_shipping_->set_listener(this);
  billing->AddChildView(use_billing_for_shipping_);

  // Container (holds label + inputs).
  views::View* container = CreateSectionContainer(
      controller_->LabelForSection(SECTION_BILLING), billing);
  billing_.container = container;
}

views::View* AutofillDialogViews::CreateInputsContainer(DialogSection section) {
  views::View* inputs_container = new views::View();
  inputs_container->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  views::View* manual_inputs = InitInputsView(section);
  inputs_container->AddChildView(manual_inputs);
  views::Combobox* combobox =
      new views::Combobox(controller_->ComboboxModelForSection(section));
  combobox->set_listener(this);
  inputs_container->AddChildView(combobox);

  DetailsGroup* group = GroupForSection(section);
  group->suggested_input = combobox;
  group->manual_input = manual_inputs;
  UpdateDetailsGroupState(*group);
  return inputs_container;
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::InitInputsView(DialogSection section) {
  const DetailInputs& inputs = controller_->RequestedFieldsForSection(section);
  TextfieldMap* textfields = &GroupForSection(section)->textfields;
  ComboboxMap* comboboxes = &GroupForSection(section)->comboboxes;

  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  for (DetailInputs::const_iterator it = inputs.begin();
       it != inputs.end(); ++it) {
    const DetailInput& input = *it;
    int kColumnSetId = input.row_id;
    views::ColumnSet* column_set = layout->GetColumnSet(kColumnSetId);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(kColumnSetId);
      if (it != inputs.begin())
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kColumnSetId);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    float expand = input.expand_weight;
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::BASELINE,
                          expand ? expand : 1,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    ui::ComboboxModel* input_model =
        controller_->ComboboxModelForAutofillType(input.type);
    // TODO(estade): TextFields and Comboboxes need to be the same height.
    if (input_model) {
      views::Combobox* combobox = new views::Combobox(input_model);
      comboboxes->insert(std::make_pair(&input, combobox));
      layout->AddView(combobox);

      for (int i = 0; i < input_model->GetItemCount(); ++i) {
        if (input.autofilled_value == input_model->GetItemAt(i)) {
          combobox->SetSelectedIndex(i);
          break;
        }
      }
    } else {
      views::Textfield* field = new views::Textfield();
      field->set_placeholder_text(ASCIIToUTF16(input.placeholder_text));
      field->SetText(input.autofilled_value);
      field->SetController(this);
      textfields->insert(std::make_pair(&input, field));
      layout->AddView(field);
    }
  }

  return view;
}

void AutofillDialogViews::UpdateDetailsGroupState(const DetailsGroup& group) {
  views::Combobox* combobox = group.suggested_input;
  int suggestion_count = combobox->model()->GetItemCount();
  bool show_combobox = suggestion_count > 1 &&
      combobox->selected_index() != suggestion_count - 1;

  combobox->SetVisible(show_combobox);
  group.manual_input->SetVisible(!show_combobox);
}

AutofillDialogViews::DetailsGroup* AutofillDialogViews::
    GroupForSection(DialogSection section) {
  switch (section) {
    case SECTION_EMAIL:
      return &email_;
    case SECTION_CC:
      return &cc_;
    case SECTION_BILLING:
      return &billing_;
    case SECTION_SHIPPING:
      return &shipping_;
  }

  NOTREACHED();
  return NULL;
}

AutofillDialogViews::DetailsGroup::DetailsGroup()
    : container(NULL),
      suggested_input(NULL),
      manual_input(NULL) {}

AutofillDialogViews::DetailsGroup::~DetailsGroup() {}

}  // namespace autofill
