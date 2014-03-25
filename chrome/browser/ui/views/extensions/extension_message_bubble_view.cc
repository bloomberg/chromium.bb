// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_message_bubble.h"
#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "grit/locale_settings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace extensions {

namespace {

// Layout constants.
const int kExtensionListPadding = 10;
const int kInsetBottomRight = 13;
const int kInsetLeft = 14;
const int kInsetTop = 9;
const int kHeadlineMessagePadding = 4;
const int kHeadlineRowPadding = 10;
const int kMessageBubblePadding = 11;

// How many extensions to show in the bubble (max).
const size_t kMaxExtensionsToShow = 7;

// How long to wait until showing the bubble (in seconds).
const int kBubbleAppearanceWaitTime = 5;

// This is a class that implements the UI for the bubble showing which
// extensions look suspicious and have therefore been automatically disabled.
class ExtensionMessageBubbleView : public ExtensionMessageBubble,
                                   public views::BubbleDelegateView,
                                   public views::ButtonListener,
                                   public views::LinkListener {
 public:
  ExtensionMessageBubbleView(
      views::View* anchor_view,
      scoped_ptr<ExtensionMessageBubbleController> controller);

  // ExtensionMessageBubble methods.
  virtual void OnActionButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnDismissButtonClicked(const base::Closure& callback) OVERRIDE;
  virtual void OnLinkClicked(const base::Closure& callback) OVERRIDE;
  virtual void Show() OVERRIDE;

  // WidgetObserver methods.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  virtual ~ExtensionMessageBubbleView();

  // Shows the bubble and updates the counter for how often it has been shown.
  void ShowBubble();

  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // views::View implementation.
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      OVERRIDE;

  base::WeakPtrFactory<ExtensionMessageBubbleView> weak_factory_;

  // The controller for this bubble.
  scoped_ptr<ExtensionMessageBubbleController> controller_;

  // The headline, labels and buttons on the bubble.
  views::Label* headline_;
  views::Link* learn_more_;
  views::LabelButton* action_button_;
  views::LabelButton* dismiss_button_;

  // All actions (link, button, esc) close the bubble, but we need to
  // make sure we don't send dismiss if the link was clicked.
  bool link_clicked_;
  bool action_taken_;

  // Callbacks into the controller.
  base::Closure action_callback_;
  base::Closure dismiss_callback_;
  base::Closure link_callback_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleView);
};

ExtensionMessageBubbleView::ExtensionMessageBubbleView(
    views::View* anchor_view,
    scoped_ptr<ExtensionMessageBubbleController> controller)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      weak_factory_(this),
      controller_(controller.Pass()),
      headline_(NULL),
      learn_more_(NULL),
      dismiss_button_(NULL),
      link_clicked_(false),
      action_taken_(false) {
  DCHECK(anchor_view->GetWidget());
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
  set_close_on_esc(true);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
}

void ExtensionMessageBubbleView::OnActionButtonClicked(
    const base::Closure& callback) {
  action_callback_ = callback;
}

void ExtensionMessageBubbleView::OnDismissButtonClicked(
    const base::Closure& callback) {
  dismiss_callback_ = callback;
}

void ExtensionMessageBubbleView::OnLinkClicked(
    const base::Closure& callback) {
  link_callback_ = callback;
}

void ExtensionMessageBubbleView::Show() {
  // Not showing the bubble right away (during startup) has a few benefits:
  // We don't have to worry about focus being lost due to the Omnibox (or to
  // other things that want focus at startup). This allows Esc to work to close
  // the bubble and also solves the keyboard accessibility problem that comes
  // with focus being lost (we don't have a good generic mechanism of injecting
  // bubbles into the focus cycle). Another benefit of delaying the show is
  // that fade-in works (the fade-in isn't apparent if the the bubble appears at
  // startup).
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ExtensionMessageBubbleView::ShowBubble,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kBubbleAppearanceWaitTime));
}

void ExtensionMessageBubbleView::OnWidgetDestroying(views::Widget* widget) {
  // To catch Esc, we monitor destroy message. Unless the link has been clicked,
  // we assume Dismiss was the action taken.
  if (!link_clicked_ && !action_taken_)
    dismiss_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleView - private.

ExtensionMessageBubbleView::~ExtensionMessageBubbleView() {}

void ExtensionMessageBubbleView::ShowBubble() {
  GetWidget()->Show();
}

void ExtensionMessageBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTop, kInsetLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  ExtensionMessageBubbleController::Delegate* delegate =
      controller_->delegate();

  const int headline_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(headline_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, headline_column_set_id);

  headline_ = new views::Label(delegate->GetTitle(),
                               rb.GetFontList(ui::ResourceBundle::MediumFont));
  layout->AddView(headline_);

  layout->AddPaddingRow(0, kHeadlineRowPadding);

  const int text_column_set_id = 1;
  views::ColumnSet* upper_columns = layout->AddColumnSet(text_column_set_id);
  upper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, text_column_set_id);

  views::Label* message = new views::Label();
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetText(delegate->GetMessageBody());
  message->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
  layout->AddView(message);

  if (delegate->ShouldShowExtensionList()) {
    const int extension_list_column_set_id = 2;
    views::ColumnSet* middle_columns =
        layout->AddColumnSet(extension_list_column_set_id);
    middle_columns->AddPaddingColumn(0, kExtensionListPadding);
    middle_columns->AddColumn(
        views::GridLayout::LEADING, views::GridLayout::CENTER,
        0, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRowWithPadding(0, extension_list_column_set_id,
        0, kHeadlineMessagePadding);
    views::Label* extensions = new views::Label();
    extensions->SetMultiLine(true);
    extensions->SetHorizontalAlignment(gfx::ALIGN_LEFT);

    std::vector<base::string16> extension_list;
    base::char16 bullet_point = 0x2022;

    std::vector<base::string16> suspicious = controller_->GetExtensionList();
    size_t i = 0;
    for (; i < suspicious.size() && i < kMaxExtensionsToShow; ++i) {
      // Add each extension with bullet point.
      extension_list.push_back(
          bullet_point + base::ASCIIToUTF16(" ") + suspicious[i]);
    }

    if (i > kMaxExtensionsToShow) {
      base::string16 difference = base::IntToString16(i - kMaxExtensionsToShow);
      extension_list.push_back(bullet_point + base::ASCIIToUTF16(" ") +
          delegate->GetOverflowText(difference));
    }

    extensions->SetText(JoinString(extension_list, base::ASCIIToUTF16("\n")));
    extensions->SizeToFit(views::Widget::GetLocalizedContentsWidth(
        IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
    layout->AddView(extensions);
  }

  base::string16 action_button = delegate->GetActionButtonLabel();

  const int action_row_column_set_id = 3;
  views::ColumnSet* bottom_columns =
      layout->AddColumnSet(action_row_column_set_id);
  bottom_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(1, 0);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  if (!action_button.empty()) {
    bottom_columns->AddColumn(views::GridLayout::TRAILING,
        views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  }
  layout->StartRowWithPadding(0, action_row_column_set_id,
                              0, kMessageBubblePadding);

  learn_more_ = new views::Link(delegate->GetLearnMoreLabel());
  learn_more_->set_listener(this);
  layout->AddView(learn_more_);

  if (!action_button.empty()) {
    action_button_ = new views::LabelButton(this, action_button.c_str());
    action_button_->SetStyle(views::Button::STYLE_BUTTON);
    layout->AddView(action_button_);
  }

  dismiss_button_ = new views::LabelButton(this,
      delegate->GetDismissButtonLabel());
  dismiss_button_->SetStyle(views::Button::STYLE_BUTTON);
  layout->AddView(dismiss_button_);
}

void ExtensionMessageBubbleView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  if (sender == action_button_) {
    action_taken_ = true;
    action_callback_.Run();
  } else {
    DCHECK_EQ(dismiss_button_, sender);
  }
  GetWidget()->Close();
}

void ExtensionMessageBubbleView::LinkClicked(views::Link* source,
                                             int event_flags) {
  DCHECK_EQ(learn_more_, source);
  link_clicked_ = true;
  link_callback_.Run();
  GetWidget()->Close();
}

void ExtensionMessageBubbleView::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_ALERT;
}

void ExtensionMessageBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleFactory

ExtensionMessageBubbleFactory::ExtensionMessageBubbleFactory(
    Profile* profile,
    ToolbarView* toolbar_view)
    : profile_(profile),
      toolbar_view_(toolbar_view),
      shown_suspicious_extensions_bubble_(false),
      shown_dev_mode_extensions_bubble_(false),
      is_observing_(false),
      stage_(STAGE_START),
      container_(NULL),
      anchor_view_(NULL) {}

ExtensionMessageBubbleFactory::~ExtensionMessageBubbleFactory() {
  MaybeStopObserving();
}

void ExtensionMessageBubbleFactory::MaybeShow(views::View* anchor_view) {
  // The list of suspicious extensions takes priority over the dev mode bubble,
  // since that needs to be shown as soon as we disable something. The dev mode
  // bubble is not as time sensitive so we'll catch the dev mode extensions on
  // the next startup/next window that opens. That way, we're not too spammy
  // with the bubbles.
  if (!shown_suspicious_extensions_bubble_) {
    if (MaybeShowSuspiciousExtensionsBubble(anchor_view))
      return;
  }

  if (!shown_dev_mode_extensions_bubble_)
    MaybeShowDevModeExtensionsBubble(anchor_view);
}

bool ExtensionMessageBubbleFactory::MaybeShowSuspiciousExtensionsBubble(
    views::View* anchor_view) {
  DCHECK(!shown_suspicious_extensions_bubble_);

  scoped_ptr<SuspiciousExtensionBubbleController> suspicious_extensions(
      new SuspiciousExtensionBubbleController(profile_));
  if (!suspicious_extensions->ShouldShow())
    return false;

  shown_suspicious_extensions_bubble_ = true;
  SuspiciousExtensionBubbleController* weak_controller =
      suspicious_extensions.get();
  ExtensionMessageBubbleView* bubble_delegate = new ExtensionMessageBubbleView(
      anchor_view,
      suspicious_extensions.PassAs<ExtensionMessageBubbleController>());

  views::BubbleDelegateView::CreateBubble(bubble_delegate);
  weak_controller->Show(bubble_delegate);

  return true;
}

bool ExtensionMessageBubbleFactory::MaybeShowDevModeExtensionsBubble(
    views::View* anchor_view) {
  DCHECK(!shown_dev_mode_extensions_bubble_);

  // Check the Developer Mode extensions.
  scoped_ptr<DevModeBubbleController> dev_mode_extensions(
      new DevModeBubbleController(profile_));

  // Return early if we have none to show.
  if (!dev_mode_extensions->ShouldShow())
    return false;

  shown_dev_mode_extensions_bubble_ = true;

  // We should be in the start stage (i.e., should not have a pending attempt to
  // show a bubble).
  DCHECK_EQ(stage_, STAGE_START);

  // Prepare to display and highlight the developer mode extensions before
  // showing the bubble. Since this is an asynchronous process, set member
  // variables for later use.
  controller_ = dev_mode_extensions.Pass();
  anchor_view_ = anchor_view;
  container_ = toolbar_view_->browser_actions();

  if (container_->animating())
    MaybeObserve();
  else
    HighlightDevModeExtensions();

  return true;
}

void ExtensionMessageBubbleFactory::MaybeObserve() {
  if (!is_observing_) {
    is_observing_ = true;
    container_->AddObserver(this);
  }
}

void ExtensionMessageBubbleFactory::MaybeStopObserving() {
  if (is_observing_) {
    is_observing_ = false;
    container_->RemoveObserver(this);
  }
}

void ExtensionMessageBubbleFactory::OnBrowserActionsContainerAnimationEnded() {
  MaybeStopObserving();
  if (stage_ == STAGE_START) {
    HighlightDevModeExtensions();
  } else if (stage_ == STAGE_HIGHLIGHTED) {
    ShowDevModeBubble();
  } else {  // We shouldn't be observing if we've completed the process.
    NOTREACHED();
    Finish();
  }
}

void ExtensionMessageBubbleFactory::OnBrowserActionsContainerDestroyed() {
  // If the container associated with the bubble is destroyed, abandon the
  // process.
  Finish();
}

void ExtensionMessageBubbleFactory::HighlightDevModeExtensions() {
  DCHECK_EQ(STAGE_START, stage_);
  stage_ = STAGE_HIGHLIGHTED;

  const ExtensionIdList extension_list = controller_->GetExtensionIdList();
  DCHECK(!extension_list.empty());
  ExtensionToolbarModel::Get(profile_)->HighlightExtensions(extension_list);
  if (container_->animating())
    MaybeObserve();
  else
    ShowDevModeBubble();
}

void ExtensionMessageBubbleFactory::ShowDevModeBubble() {
  DCHECK_EQ(stage_, STAGE_HIGHLIGHTED);
  stage_ = STAGE_COMPLETE;

  views::View* reference_view = NULL;
  if (container_->num_browser_actions() > 0)
    reference_view = container_->GetBrowserActionViewAt(0);
  if (reference_view && reference_view->visible())
    anchor_view_ = reference_view;

  DevModeBubbleController* weak_controller = controller_.get();
  ExtensionMessageBubbleView* bubble_delegate = new ExtensionMessageBubbleView(
      anchor_view_,
      scoped_ptr<ExtensionMessageBubbleController>(controller_.release()));
  views::BubbleDelegateView::CreateBubble(bubble_delegate);
  weak_controller->Show(bubble_delegate);

  Finish();
}

void ExtensionMessageBubbleFactory::Finish() {
  MaybeStopObserving();
  controller_.reset();
  anchor_view_ = NULL;
  container_ = NULL;
}

}  // namespace extensions
