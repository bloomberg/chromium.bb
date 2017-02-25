// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/settings_reset_prompt_dialog.h"

#include <vector>

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace safe_browsing {

// static
void SettingsResetPromptController::ShowSettingsResetPrompt(
    Browser* browser,
    SettingsResetPromptController* controller) {
  SettingsResetPromptDialog* dialog = new SettingsResetPromptDialog(controller);
  // The dialog will delete itself, as implemented in
  // |DialogDelegateView::DeleteDelegate()|, when its widget is closed.
  dialog->Show(browser);
}

}  // namespace safe_browsing

namespace {

using LabelInfo = safe_browsing::SettingsResetPromptController::LabelInfo;

constexpr int kDialogWidth = 512;
constexpr int kDetailsSectionMaxHeight = 150;
constexpr int kBulletColumnWidth = 5;
// Constants used for the layout of the label views.
static constexpr int kMainColumSetId = 0;
static constexpr int kBulletColumnSetId = 1;

// Helper function used by |CreateLabelView()| below that adds the labels as
// specified by |labels| to |label_view|.
void AddLabelsToLabelView(views::View* label_view,
                          views::GridLayout* layout,
                          const std::vector<LabelInfo>& labels) {
  static const base::char16 kBulletPoint[] = {0x2022, 0};

  bool first_label = true;
  bool last_label_was_bullet = false;
  for (const LabelInfo& label_info : labels) {
    const bool is_bullet = label_info.type == LabelInfo::BULLET_ITEM;
    views::Label* label = new views::Label(label_info.text);
    label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (is_bullet) {
      label->SetElideBehavior(gfx::ELIDE_TAIL);
    } else {
      label->SetMultiLine(true);
    }

    // Do not add a padding row if
    // - this is the first label being added, or
    // - a bullet item is being added and the last label was also a bullet item.
    if (!(first_label || (is_bullet && last_label_was_bullet)))
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

// Creates a view that displays two types of labels: multiline labels or
// indented single-line labels that are indented and start with a bullet
// point. The indented text is intended to be used to display setting values
// (URLs) and is elided if the text is too wide to fit in its entirety.
views::View* CreateLabelView(int top_vertical_spacing,
                             const std::vector<LabelInfo>& labels) {
  views::View* label_view = new views::View();
  views::GridLayout* layout = new views::GridLayout(label_view);
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

  if (top_vertical_spacing > 0)
    layout->AddPaddingRow(/*vertical_resize=*/0, top_vertical_spacing);

  AddLabelsToLabelView(label_view, layout, labels);
  return label_view;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SettingsResetPromptDialog::ExpandableMessageView
//
// A view, whose visibilty can be toggled, and will be used for the details
// section the main dialog.
class SettingsResetPromptDialog::ExpandableMessageView : public views::View {
 public:
  explicit ExpandableMessageView(const std::vector<LabelInfo>& labels);
  ~ExpandableMessageView() override;

  void ToggleShowView();

  // views::View overrides.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExpandableMessageView);
};

SettingsResetPromptDialog::ExpandableMessageView::ExpandableMessageView(
    const std::vector<LabelInfo>& labels) {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  SetVisible(false);

  constexpr int kColumnId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnId);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                        /*resize_percent=*/1, views::GridLayout::USE_PREF,
                        /*fixed_width=*/0,
                        /*min_width=*/0);

  // Add a horizontal line separator.
  layout->StartRowWithPadding(
      /*vertical_resize=*/0, kColumnId,
      /*padding_resize=*/0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(new views::Separator());

  // Add the main message view inside a scroll view.
  views::View* label_view =
      CreateLabelView(views::kUnrelatedControlVerticalSpacing, labels);
  views::ScrollView* scroll_view = new views::ScrollView();
  scroll_view->ClipHeightTo(/*min_height=*/0, kDetailsSectionMaxHeight);
  scroll_view->SetContents(label_view);
  layout->StartRow(0, kColumnId);
  layout->AddView(scroll_view);
}

SettingsResetPromptDialog::ExpandableMessageView::~ExpandableMessageView() {}

void SettingsResetPromptDialog::ExpandableMessageView::ToggleShowView() {
  SetVisible(!visible());
  PreferredSizeChanged();
}

gfx::Size SettingsResetPromptDialog::ExpandableMessageView::GetPreferredSize()
    const {
  gfx::Size size = views::View::GetPreferredSize();
  return gfx::Size(size.width(), visible() ? size.height() : 0);
}

int SettingsResetPromptDialog::ExpandableMessageView::GetHeightForWidth(
    int width) const {
  return GetPreferredSize().height();
}

////////////////////////////////////////////////////////////////////////////////
// SettingsResetPromptDialog

SettingsResetPromptDialog::SettingsResetPromptDialog(
    safe_browsing::SettingsResetPromptController* controller)
    : browser_(nullptr),
      controller_(controller),
      interaction_done_(false),
      details_link_(nullptr) {
  DCHECK(controller_);

  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
                                        views::kButtonHEdgeMarginNew,
                                        views::kPanelVertMargin, 0));

  // Add the dialog's main label view.
  AddChildView(CreateLabelView(0, controller_->GetMainText()));

  // Add the main details view that starts off not being visible.
  details_view_ = new ExpandableMessageView(controller_->GetDetailsText());
  AddChildView(details_view_);
}

SettingsResetPromptDialog::~SettingsResetPromptDialog() {
  // Make sure the controller is correctly notified in case the dialog widget is
  // closed by some other means than the dialog buttons.
  Close();
}

void SettingsResetPromptDialog::Show(Browser* browser) {
  DCHECK(browser);
  browser_ = browser;
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  constrained_window::CreateBrowserModalDialogViews(
      this, browser_view->GetNativeWindow())
      ->Show();
}

// WidgetDelegate overrides.

ui::ModalType SettingsResetPromptDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

bool SettingsResetPromptDialog::ShouldShowWindowIcon() const {
  return true;
}

gfx::ImageSkia SettingsResetPromptDialog::GetWindowIcon() {
  return gfx::CreateVectorIcon(gfx::VectorIconId::WARNING, 32 /* Icon size. */,
                               gfx::kGoogleRed700);
}

base::string16 SettingsResetPromptDialog::GetWindowTitle() const {
  return controller_->GetWindowTitle();
}

// DialogModel overrides.

bool SettingsResetPromptDialog::ShouldDefaultButtonBeBlue() const {
  return true;
}

// DialogDelegate overrides.

base::string16 SettingsResetPromptDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);

  if (button == ui::DIALOG_BUTTON_OK)
    return controller_->GetButtonLabel();
  return DialogDelegate::GetDialogButtonLabel(button);
}

views::View* SettingsResetPromptDialog::CreateExtraView() {
  // Add a "show details" link that toggles visibility of the details view.
  details_link_ = new views::Link(controller_->GetShowDetailsLabel());
  details_link_->SetUnderline(false);
  details_link_->set_listener(this);
  return details_link_;
}

bool SettingsResetPromptDialog::Accept() {
  if (!interaction_done_) {
    interaction_done_ = true;
    controller_->Accept();
  }
  return true;
}

bool SettingsResetPromptDialog::Cancel() {
  if (!interaction_done_) {
    interaction_done_ = true;
    controller_->Cancel();
  }
  return true;
}

// View overrides.

gfx::Size SettingsResetPromptDialog::GetPreferredSize() const {
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

// LinkListener overrides.

void SettingsResetPromptDialog::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(source, details_link_);
  DCHECK(browser_);

  details_view_->ToggleShowView();
  details_link_->SetText(details_view_->visible()
                             ? controller_->GetHideDetailsLabel()
                             : controller_->GetShowDetailsLabel());

  ChromeWebModalDialogManagerDelegate* manager = browser_;
  constrained_window::UpdateWidgetModalDialogPosition(
      GetWidget(), manager->GetWebContentsModalDialogHost());
}
