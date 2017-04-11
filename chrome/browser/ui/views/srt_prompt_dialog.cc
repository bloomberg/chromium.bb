// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/srt_prompt_dialog.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/safe_browsing/srt_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace chrome {

void ShowSRTPrompt(Browser* browser,
                   safe_browsing::SRTPromptController* controller) {
  SRTPromptDialog* dialog = new SRTPromptDialog(controller);
  dialog->Show(browser);
}

}  // namespace chrome

namespace {

using LabelInfo = safe_browsing::SRTPromptController::LabelInfo;

constexpr int kDialogWidth = 448;
constexpr int kDetailsSectionMaxHeight = 150;
constexpr int kBulletColumnWidth = 10;
// Constants used for the layout of the label views.
constexpr int kMainColumSetId = 0;
constexpr int kBulletColumnSetId = 1;

// Returns a view containing |item| with insets defined by |top|, |left|,
// |bottom|, and |right|.
views::View* CreateViewWithInsets(views::View* item,
                                  int top,
                                  int left,
                                  int bottom,
                                  int right) {
  views::View* view = new views::View();
  view->SetLayoutManager(new views::FillLayout());
  view->SetBorder(views::CreateEmptyBorder(top, left, bottom, right));
  view->AddChildView(item);
  return view;
}

// Helper function used by |CreateLabelView()| below that adds |labels| to
// |label_view|.
void AddLabelsToLabelView(views::View* label_view,
                          views::GridLayout* layout,
                          const std::vector<LabelInfo>& labels) {
  static constexpr base::char16 kBulletPoint[] = {0x2022, 0};

  bool first_label = true;
  bool last_label_was_bullet = false;
  for (const LabelInfo& label_info : labels) {
    const bool is_bullet = label_info.type == LabelInfo::BULLET_ITEM;
    views::Label* label = new views::Label(label_info.text);
    label->SetMultiLine(true);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    // Do not add a padding row if
    // - this is the first label being added, or
    // - a bullet item is being added and the last label was also a bullet item.
    bool skip_padding = first_label || (is_bullet && last_label_was_bullet);
    if (!skip_padding)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

    layout->StartRow(0, is_bullet ? kBulletColumnSetId : kMainColumSetId);
    if (is_bullet) {
      views::Label* bullet = new views::Label(base::string16(kBulletPoint));
      layout->AddView(bullet);
    }
    layout->AddView(label);

    last_label_was_bullet = is_bullet;
    first_label = false;
  }
}

// Creates a view that displays two types of labels: multiline labels
// representing whole paragraphs or indented labels that are start with a bullet
// point.
//
// A vertical padding of size |top_vertical_space| is added before the labels.
views::View* CreateLabelView(int top_vertical_space,
                             const std::vector<LabelInfo>& labels) {
  views::View* label_view = new views::View();
  views::GridLayout* layout = new views::GridLayout(label_view);
  layout->SetInsets(0, views::kButtonHEdgeMarginNew, 0,
                    views::kButtonHEdgeMarginNew);

  label_view->SetLayoutManager(layout);

  views::ColumnSet* main_column_set = layout->AddColumnSet(kMainColumSetId);
  main_column_set->AddColumn(views::GridLayout::FILL,
                             views::GridLayout::LEADING,
                             /*resize_percent=*/1, views::GridLayout::USE_PREF,
                             /*fixed_width=*/0,
                             /*min_width=*/0);

  views::ColumnSet* bullet_column_set_ =
      layout->AddColumnSet(kBulletColumnSetId);
  bullet_column_set_->AddPaddingColumn(
      /*resize_percent=*/0, views::kUnrelatedControlLargeHorizontalSpacing);
  bullet_column_set_->AddColumn(
      views::GridLayout::FILL, views::GridLayout::LEADING,
      /*resize_percent=*/0, views::GridLayout::USE_PREF,
      /*fixed_width=*/0,
      /*min_width=*/0);
  bullet_column_set_->AddPaddingColumn(/*resize_percent=*/0,
                                       kBulletColumnWidth);
  bullet_column_set_->AddColumn(
      views::GridLayout::FILL, views::GridLayout::LEADING,
      /*resize_percent=*/1, views::GridLayout::USE_PREF,
      /*fixed_width=*/0,
      /*min_width=*/0);

  if (top_vertical_space > 0)
    layout->AddPaddingRow(/*vertical_resize=*/0, top_vertical_space);

  AddLabelsToLabelView(label_view, layout, labels);

  layout->AddPaddingRow(/*vertical_resize=*/0,
                        views::kUnrelatedControlLargeHorizontalSpacing);
  return label_view;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SRTPromptDialog::ExpandableMessageView
//
// A view, whose visibilty can be toggled, and will be used for the details
// section the main dialog.
class SRTPromptDialog::ExpandableMessageView : public views::View {
 public:
  explicit ExpandableMessageView(const std::vector<LabelInfo>& labels);
  ~ExpandableMessageView() override;

  void AnimateToState(double state);

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;

 private:
  // A number between 0 and 1 that determines how much of the view's preferred
  // height should be visible.
  double animation_state_;

  DISALLOW_COPY_AND_ASSIGN(ExpandableMessageView);
};

SRTPromptDialog::ExpandableMessageView::ExpandableMessageView(
    const std::vector<LabelInfo>& labels)
    : animation_state_(0.0) {
  // Add the main message view inside a scroll view.
  views::View* label_view =
      CreateLabelView(views::kUnrelatedControlLargeHorizontalSpacing, labels);
  views::ScrollView* scroll_view = new views::ScrollView();
  scroll_view->ClipHeightTo(kDetailsSectionMaxHeight, kDetailsSectionMaxHeight);
  scroll_view->SetContents(label_view);
  scroll_view->SetSize(gfx::Size(kDialogWidth, kDetailsSectionMaxHeight));
  AddChildView(scroll_view);
}

SRTPromptDialog::ExpandableMessageView::~ExpandableMessageView() {}

void SRTPromptDialog::ExpandableMessageView::AnimateToState(double state) {
  DCHECK_LE(0.0, state);
  DCHECK_GE(1.0, state);

  animation_state_ = state;
  PreferredSizeChanged();
}

gfx::Size SRTPromptDialog::ExpandableMessageView::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, kDetailsSectionMaxHeight * animation_state_);
}

int SRTPromptDialog::ExpandableMessageView::GetHeightForWidth(int width) const {
  return GetPreferredSize().height();
}

////////////////////////////////////////////////////////////////////////////////
// SRTPromptDialog

SRTPromptDialog::SRTPromptDialog(safe_browsing::SRTPromptController* controller)
    : browser_(nullptr),
      controller_(controller),
      slide_animation_(base::MakeUnique<gfx::SlideAnimation>(this)),
      details_view_(new ExpandableMessageView(controller_->GetDetailsText())),
      details_button_(
          new views::LabelButton(this, controller_->GetShowDetailsLabel())) {
  DCHECK(controller_);

  SetLayoutManager(new views::BoxLayout(
      /*orientation=*/views::BoxLayout::kVertical,
      /*inside_border_horizontal_spacing=*/0,
      /*inside_border_vertical_spacing=*/views::kPanelVertMargin,
      /*between_child_spacing=*/0));

  AddChildView(CreateLabelView(0, controller_->GetMainText()));
  AddChildView(new views::Separator());

  AddChildView(details_view_);

  details_button_->SetEnabledTextColors(GetDetailsButtonColor());
  UpdateDetailsButton();
  AddChildView(CreateViewWithInsets(
      details_button_, views::kPanelVertMargin, views::kButtonHEdgeMarginNew,
      views::kPanelVertMargin, views::kButtonHEdgeMarginNew));
  AddChildView(new views::Separator());
}

SRTPromptDialog::~SRTPromptDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  if (controller_)
    controller_->Cancel();
}

void SRTPromptDialog::Show(Browser* browser) {
  DCHECK(browser);
  DCHECK(!browser_);
  DCHECK(controller_);

  browser_ = browser;
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_->window()->GetNativeWindow())
      ->Show();
  controller_->DialogShown();
}

// DialogModel overrides.

bool SRTPromptDialog::ShouldDefaultButtonBeBlue() const {
  return true;
}

// WidgetDelegate overrides.

ui::ModalType SRTPromptDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 SRTPromptDialog::GetWindowTitle() const {
  DCHECK(controller_);
  return controller_->GetWindowTitle();
}

// DialogDelegate overrides.

base::string16 SRTPromptDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
  DCHECK(controller_);

  if (button == ui::DIALOG_BUTTON_OK)
    return controller_->GetAcceptButtonLabel();
  return DialogDelegate::GetDialogButtonLabel(button);
}

bool SRTPromptDialog::Accept() {
  if (controller_) {
    controller_->Accept();
    controller_ = nullptr;
  }
  return true;
}

bool SRTPromptDialog::Cancel() {
  if (controller_) {
    controller_->Cancel();
    controller_ = nullptr;
  }
  return true;
}

// View overrides.

gfx::Size SRTPromptDialog::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

// views::ButtonListener overrides.

void SRTPromptDialog::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  DCHECK_EQ(sender, details_button_);
  DCHECK(browser_);

  if (slide_animation_->IsShowing())
    slide_animation_->Hide();
  else
    slide_animation_->Show();
}

void SRTPromptDialog::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK_EQ(slide_animation_.get(), animation);

  details_view_->AnimateToState(animation->GetCurrentValue());
  ChromeWebModalDialogManagerDelegate* manager = browser_;
  constrained_window::UpdateWidgetModalDialogPosition(
      GetWidget(), manager->GetWebContentsModalDialogHost());
}

void SRTPromptDialog::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(slide_animation_.get(), animation);
  UpdateDetailsButton();
}

SkColor SRTPromptDialog::GetDetailsButtonColor() {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LinkEnabled);
}

void SRTPromptDialog::UpdateDetailsButton() {
  DCHECK(controller_);
  details_button_->SetText(slide_animation_->IsShowing()
                               ? controller_->GetHideDetailsLabel()
                               : controller_->GetShowDetailsLabel());
  details_button_->SetImage(
      views::Button::STATE_NORMAL,
      slide_animation_->IsShowing()
          ? gfx::CreateVectorIcon(kCaretUpIcon, GetDetailsButtonColor())
          : gfx::CreateVectorIcon(kCaretDownIcon, GetDetailsButtonColor()));
}
