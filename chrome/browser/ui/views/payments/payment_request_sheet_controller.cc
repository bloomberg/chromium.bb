// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

namespace {

// This event is used to pass to ButtonPressed when its event parameter doesn't
// matter, only the sender.
class DummyEvent : public ui::Event {
 public:
  DummyEvent() : ui::Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
};

// This class is the actual sheet that gets pushed on the view_stack_. It
// implements views::FocusTraversable to trap focus within its hierarchy. This
// way, focus doesn't leave the topmost sheet on the view stack to go on views
// that happen to be underneath.
// This class also overrides RequestFocus() to allow consumers to specify which
// view should be focused first (through SetFirstFocusableView). If no initial
// view is specified, the first view added to the hierarchy will get focus when
// this SheetView's RequestFocus() is called.
class SheetView : public views::View, public views::FocusTraversable {
 public:
  explicit SheetView(
      const base::Callback<bool()>& enter_key_accelerator_callback)
      : first_focusable_(nullptr),
        focus_search_(base::MakeUnique<views::FocusSearch>(this, true, false)),
        enter_key_accelerator_(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE)),
        enter_key_accelerator_callback_(enter_key_accelerator_callback) {
    if (enter_key_accelerator_callback_)
      AddAccelerator(enter_key_accelerator_);
  }

  // Sets |view| as the first focusable view in this pane. If it's nullptr, the
  // fallback is to use focus_search_ to find the first focusable view.
  void SetFirstFocusableView(views::View* view) { first_focusable_ = view; }

 private:
  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override { return focus_search_.get(); }

  views::FocusTraversable* GetFocusTraversableParent() override {
    return parent()->GetFocusTraversable();
  }

  views::View* GetFocusTraversableParentView() override { return this; }

  // views::View:
  views::FocusTraversable* GetPaneFocusTraversable() override { return this; }

  void RequestFocus() override {
    views::View* first_focusable = first_focusable_;

    if (!first_focusable) {
      views::FocusTraversable* dummy_focus_traversable;
      views::View* dummy_focus_traversable_view;
      first_focusable = focus_search_->FindNextFocusableView(
          nullptr, false, views::FocusSearch::DOWN, false,
          &dummy_focus_traversable, &dummy_focus_traversable_view);
    }

    if (first_focusable)
      first_focusable->RequestFocus();
  }

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    if (accelerator == enter_key_accelerator_ &&
        enter_key_accelerator_callback_) {
      return enter_key_accelerator_callback_.Run();
    }
    return views::View::AcceleratorPressed(accelerator);
  }

  views::View* first_focusable_;
  std::unique_ptr<views::FocusSearch> focus_search_;
  ui::Accelerator enter_key_accelerator_;
  base::Callback<bool()> enter_key_accelerator_callback_;

  DISALLOW_COPY_AND_ASSIGN(SheetView);
};

}  // namespace

PaymentRequestSheetController::PaymentRequestSheetController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : spec_(spec), state_(state), dialog_(dialog) {}

PaymentRequestSheetController::~PaymentRequestSheetController() {}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateView() {
  // Create the footer now so that it's known if there's a primary button or not
  // before creating the sheet view. This way, it's possible to determine
  // whether there's something to do when the user hits enter.
  std::unique_ptr<views::View> footer = CreateFooterView();
  std::unique_ptr<SheetView> view = base::MakeUnique<SheetView>(
      primary_button_
          ? base::Bind(
                &PaymentRequestSheetController::PerformPrimaryButtonAction,
                base::Unretained(this))
          : base::Callback<bool()>());

  DialogViewID sheet_id;
  if (GetSheetId(&sheet_id))
    view->set_id(static_cast<int>(sheet_id));

  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);

  // Note: each view is responsible for its own padding (insets).
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  // |header_view| will be deleted when |view| is.
  layout->AddView(
      CreateSheetHeaderView(ShouldShowHeaderBackArrow(), GetSheetTitle(), this)
          .release());

  layout->StartRow(1, 0);
  // |content_view| will go into a views::ScrollView so it needs to be sized now
  // otherwise it'll be sized to the ScrollView's viewport height, preventing
  // the scroll bar from ever being shown.
  pane_ = new views::View;
  views::GridLayout* pane_layout = new views::GridLayout(pane_);
  views::ColumnSet* pane_columns = pane_layout->AddColumnSet(0);
  pane_columns->AddColumn(views::GridLayout::Alignment::FILL,
                          views::GridLayout::Alignment::LEADING, 0,
                          views::GridLayout::SizeType::FIXED,
                          GetActualDialogWidth(), GetActualDialogWidth());
  pane_->SetLayoutManager(pane_layout);
  pane_layout->StartRow(0, 0);
  // This is owned by its parent. It's the container passed to FillContentView.
  content_view_ = new views::View;
  pane_layout->AddView(content_view_);
  pane_->SizeToPreferredSize();

  scroll_ = base::MakeUnique<views::ScrollView>();
  scroll_->set_owned_by_client();
  scroll_->EnableViewPortLayer();
  scroll_->set_hide_horizontal_scrollbar(true);
  scroll_->SetContents(pane_);
  layout->AddView(scroll_.get());

  layout->StartRow(0, 0);
  layout->AddView(footer.release());

  UpdateContentView();

  view->SetFirstFocusableView(GetFirstFocusedView());
  return std::move(view);
}

void PaymentRequestSheetController::UpdateContentView() {
  content_view_->RemoveAllChildViews(true);
  FillContentView(content_view_);
  RelayoutPane();
}

void PaymentRequestSheetController::RelayoutPane() {
  content_view_->Layout();
  pane_->SizeToPreferredSize();
  // Now that the content and its surrounding pane are updated, force a Layout
  // on the ScrollView so that it updates its scroll bars now.
  scroll_->Layout();
}

std::unique_ptr<views::Button>
PaymentRequestSheetController::CreatePrimaryButton() {
  return nullptr;
}

base::string16 PaymentRequestSheetController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool PaymentRequestSheetController::ShouldShowHeaderBackArrow() {
  return true;
}

std::unique_ptr<views::View>
PaymentRequestSheetController::CreateExtraFooterView() {
  return nullptr;
}

void PaymentRequestSheetController::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  switch (static_cast<PaymentRequestCommonTags>(sender->tag())) {
    case PaymentRequestCommonTags::CLOSE_BUTTON_TAG:
      dialog()->CloseDialog();
      break;
    case PaymentRequestCommonTags::BACK_BUTTON_TAG:
      dialog()->GoBack();
      break;
    case PaymentRequestCommonTags::PAY_BUTTON_TAG:
      dialog()->Pay();
      break;
    case PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX:
      NOTREACHED();
      break;
  }
}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateFooterView() {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  // The horizontal distance between the right/left edges of the dialog and the
  // elements.
  constexpr int kFooterHorizontalInset = 16;
  // The vertical distance between footer elements and the top/bottom border
  // (the bottom border is the edge of the dialog).
  constexpr int kFooterVerticalInset = 16;
  layout->SetInsets(kFooterVerticalInset, kFooterHorizontalInset,
                    kFooterVerticalInset, kFooterHorizontalInset);
  layout->StartRow(0, 0);
  std::unique_ptr<views::View> extra_view = CreateExtraFooterView();
  if (extra_view)
    layout->AddView(extra_view.release());
  else
    layout->SkipColumns(1);

  std::unique_ptr<views::View> trailing_buttons_container =
      base::MakeUnique<views::View>();

  trailing_buttons_container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kPaymentRequestButtonSpacing));

  primary_button_ = CreatePrimaryButton();
  if (primary_button_) {
    primary_button_->set_owned_by_client();
    trailing_buttons_container->AddChildView(primary_button_.get());
  }

  secondary_button_ = std::unique_ptr<views::Button>(
      views::MdTextButton::CreateSecondaryUiButton(this,
                                                   GetSecondaryButtonLabel()));
  secondary_button_->set_owned_by_client();
  secondary_button_->set_tag(
      static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG));
  secondary_button_->set_id(static_cast<int>(DialogViewID::CANCEL_BUTTON));
  trailing_buttons_container->AddChildView(secondary_button_.get());

  layout->AddView(trailing_buttons_container.release());

  return container;
}

views::View* PaymentRequestSheetController::GetFirstFocusedView() {
  if (primary_button_ && primary_button_->enabled())
    return primary_button_.get();

  DCHECK(secondary_button_);

  return secondary_button_.get();
}

bool PaymentRequestSheetController::GetSheetId(DialogViewID* sheet_id) {
  return false;
}

bool PaymentRequestSheetController::PerformPrimaryButtonAction() {
  if (primary_button_ && primary_button_->enabled()) {
    ButtonPressed(primary_button_.get(), DummyEvent());
    return true;
  }
  return false;
}

}  // namespace payments
