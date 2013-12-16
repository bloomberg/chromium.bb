// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_message_bubble_view.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/dev_mode_bubble_controller.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "grit/locale_settings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ExtensionMessageBubbleView

namespace extensions {

ExtensionMessageBubbleView::ExtensionMessageBubbleView(
    views::View* anchor_view,
    ExtensionMessageBubbleController::Delegate* delegate)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      weak_factory_(this),
      delegate_(delegate),
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

// static
void ExtensionMessageBubbleView::MaybeShow(
    Browser* browser,
    ToolbarView* toolbar_view,
    views::View* anchor_view) {
#if defined(OS_WIN)
  // The list of suspicious extensions takes priority over the dev mode bubble,
  // since that needs to be shown as soon as we disable something. The dev mode
  // bubble is not as time sensitive so we'll catch the dev mode extensions on
  // the next startup/next window that opens. That way, we're not too spammy
  // with the bubbles.
  SuspiciousExtensionBubbleController* suspicious_extensions =
      extensions::SuspiciousExtensionBubbleController::Get(
          browser->profile());
  if (suspicious_extensions->ShouldShow()) {
    ExtensionMessageBubbleView* bubble_delegate =
        new ExtensionMessageBubbleView(anchor_view, suspicious_extensions);
    views::BubbleDelegateView::CreateBubble(bubble_delegate);
    suspicious_extensions->Show(bubble_delegate);
    return;
  }

  DevModeBubbleController* dev_mode_extensions =
      extensions::DevModeBubbleController::Get(
          browser->profile());
  if (dev_mode_extensions->ShouldShow()) {
    views::View* reference_view = NULL;
    BrowserActionsContainer* container = toolbar_view->browser_actions();
    if (container->animating())
      return;

    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser->profile())->extension_service();
    extensions::ExtensionActionManager* extension_action_manager =
        extensions::ExtensionActionManager::Get(browser->profile());

    const ExtensionIdList extension_list =
        dev_mode_extensions->GetExtensionIdList();
    ExtensionToolbarModel::Get(
        browser->profile())->EnsureVisibility(extension_list);
    for (size_t i = 0; i < extension_list.size(); ++i) {
      const Extension* extension =
          service->GetExtensionById(extension_list[i], false);
      if (!extension)
        continue;
      reference_view = container->GetBrowserActionView(
          extension_action_manager->GetBrowserAction(*extension));
      if (reference_view && reference_view->visible())
        break;  // Found a good candidate.
    }
    if (reference_view) {
      // If we have a view, it means we found a browser action and we want to
      // point to the chevron, not the hotdog menu.
      if (!reference_view->visible())
        reference_view = container->chevron();  // It's hidden, use the chevron.
    }
    if (reference_view && reference_view->visible())
      anchor_view = reference_view;  // Catch-all is the hotdog menu.

    // Show the bubble.
    ExtensionMessageBubbleView* bubble_delegate =
        new ExtensionMessageBubbleView(anchor_view, dev_mode_extensions);
    views::BubbleDelegateView::CreateBubble(bubble_delegate);
    dev_mode_extensions->Show(bubble_delegate);
  }
#endif
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

ExtensionMessageBubbleView::~ExtensionMessageBubbleView() {
}

void ExtensionMessageBubbleView::ShowBubble() {
  StartFade(true);
}

void ExtensionMessageBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTop, kInsetLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  const int headline_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(headline_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, headline_column_set_id);

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  headline_->SetText(delegate_->GetTitle());
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
  message->SetText(delegate_->GetMessageBody());
  message->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
  layout->AddView(message);

  if (delegate_->ShouldShowExtensionList()) {
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

    std::vector<string16> extension_list;
    char16 bullet_point = 0x2022;

    std::vector<string16> suspicious = delegate_->GetExtensions();
    size_t i = 0;
    for (; i < suspicious.size() && i < kMaxExtensionsToShow; ++i) {
      // Add each extension with bullet point.
      extension_list.push_back(
          bullet_point + ASCIIToUTF16(" ") + suspicious[i]);
    }

    if (i > kMaxExtensionsToShow) {
      string16 difference = base::IntToString16(i - kMaxExtensionsToShow);
      extension_list.push_back(bullet_point + ASCIIToUTF16(" ") +
          delegate_->GetOverflowText(difference));
    }

    extensions->SetText(JoinString(extension_list, ASCIIToUTF16("\n")));
    extensions->SizeToFit(views::Widget::GetLocalizedContentsWidth(
        IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
    layout->AddView(extensions);
  }

  string16 action_button = delegate_->GetActionButtonLabel();

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

  learn_more_ = new views::Link(delegate_->GetLearnMoreLabel());
  learn_more_->set_listener(this);
  layout->AddView(learn_more_);

  if (!action_button.empty()) {
    action_button_ = new views::LabelButton(this, action_button.c_str());
    action_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
    layout->AddView(action_button_);
  }

  dismiss_button_ = new views::LabelButton(this,
      delegate_->GetDismissButtonLabel());
  dismiss_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
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
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

void ExtensionMessageBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_ALERT, true);
}

}  // namespace extensions
