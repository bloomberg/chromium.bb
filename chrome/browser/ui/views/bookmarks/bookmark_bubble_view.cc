// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/user_metrics.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"

using views::ColumnSet;
using views::GridLayout;

// Padding between "Title:" and the actual title.
static const int kTitlePadding = 4;

// Minimum width for the fields - they will push out the size of the bubble if
// necessary. This should be big enough so that the field pushes the right side
// of the bubble far enough so that the edit button's left edge is to the right
// of the field's left edge.
static const int kMinimumFieldSize = 180;

// TODO(msw): Get color from theme/window color.
static const SkColor kColor = SK_ColorWHITE;

// Declared in browser_dialogs.h so callers don't have to depend on our header.

namespace browser {

void ShowBookmarkBubbleView(views::View* anchor_view,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked) {
  BookmarkBubbleView::ShowBubble(anchor_view, profile, url, newly_bookmarked);
}

void HideBookmarkBubbleView() {
  BookmarkBubbleView::Hide();
}

bool IsBookmarkBubbleViewShowing() {
  return BookmarkBubbleView::IsShowing();
}

}  // namespace browser

// BookmarkBubbleView ---------------------------------------------------------

BookmarkBubbleView* BookmarkBubbleView::bookmark_bubble_ = NULL;

// static
void BookmarkBubbleView::ShowBubble(views::View* anchor_view,
                                    Profile* profile,
                                    const GURL& url,
                                    bool newly_bookmarked) {
  if (IsShowing())
    return;

  bookmark_bubble_ =
      new BookmarkBubbleView(anchor_view, profile, url, newly_bookmarked);
  browser::CreateViewsBubble(bookmark_bubble_);
  bookmark_bubble_->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_bubble_->title_tf_->SelectAll();

  GURL url_ptr(url);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_BUBBLE_SHOWN,
      content::Source<Profile>(profile->GetOriginalProfile()),
      content::Details<GURL>(&url_ptr));
}

// static
bool BookmarkBubbleView::IsShowing() {
  return bookmark_bubble_ != NULL;
}

void BookmarkBubbleView::Hide() {
  if (IsShowing())
    bookmark_bubble_->GetWidget()->Close();
}

BookmarkBubbleView::~BookmarkBubbleView() {
  if (apply_edits_) {
    ApplyEdits();
  } else if (remove_bookmark_) {
    BookmarkModel* model = profile_->GetBookmarkModel();
    const BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
    if (node)
      model->Remove(node->parent(), node->parent()->GetIndexOf(node));
  }
}

views::View* BookmarkBubbleView::GetInitiallyFocusedView() {
  return title_tf_;
}

gfx::Point BookmarkBubbleView::GetAnchorPoint() {
  // Compensate for some built-in padding in the star image.
  return BubbleDelegateView::GetAnchorPoint().Subtract(gfx::Point(0, 5));
}

void BookmarkBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK(bookmark_bubble_ == this);
  bookmark_bubble_ = NULL;

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_BUBBLE_HIDDEN,
      content::Source<Profile>(profile_->GetOriginalProfile()),
      content::NotificationService::NoDetails());
 }

bool BookmarkBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN) {
     if (edit_button_->HasFocus())
       HandleButtonPressed(edit_button_);
     else
       HandleButtonPressed(close_button_);
     return true;
  } else if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    remove_bookmark_ = newly_bookmarked_;
    apply_edits_ = false;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void BookmarkBubbleView::Init() {
  remove_link_ = new views::Link(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK));
  remove_link_->set_listener(this);

  edit_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS));

  close_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));
  close_button_->SetIsDefault(true);

  views::Label* combobox_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_TEXT));

  parent_combobox_ = new views::Combobox(&parent_model_);
  parent_combobox_->SetSelectedItem(parent_model_.node_parent_index());
  parent_combobox_->set_listener(this);
  parent_combobox_->SetAccessibleName(combobox_label->GetText());

  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(
          newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED :
                              IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK));
  title_label->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  title_label->SetEnabledColor(SkColorSetRGB(6, 45, 117));

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  ColumnSet* cs = layout->AddColumnSet(0);

  // Top (title) row.
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);

  // Middle (input field) rows.
  cs = layout->AddColumnSet(2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                GridLayout::USE_PREF, 0, kMinimumFieldSize);

  // Bottom (buttons) row.
  cs = layout->AddColumnSet(3);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  // We subtract 2 to account for the natural button padding, and
  // to bring the separation visually in line with the row separation
  // height.
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing - 2);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(title_label);
  layout->AddView(remove_link_);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, 2);
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_TITLE_TEXT));
  layout->AddView(label);
  title_tf_ = new views::Textfield();
  title_tf_->SetText(GetTitle());
  layout->AddView(title_tf_);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 2);
  layout->AddView(combobox_label);
  layout->AddView(parent_combobox_);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(0, 3);
  layout->AddView(edit_button_);
  layout->AddView(close_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
}

BookmarkBubbleView::BookmarkBubbleView(views::View* anchor_view,
                                       Profile* profile,
                                       const GURL& url,
                                       bool newly_bookmarked)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT, kColor),
      profile_(profile),
      url_(url),
      newly_bookmarked_(newly_bookmarked),
      parent_model_(
          profile_->GetBookmarkModel(),
          profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url)),
      remove_bookmark_(false),
      apply_edits_(true) {
}

string16 BookmarkBubbleView::GetTitle() {
  BookmarkModel* bookmark_model= profile_->GetBookmarkModel();
  const BookmarkNode* node =
      bookmark_model->GetMostRecentlyAddedNodeForURL(url_);
  if (node)
    return node->GetTitle();
  else
    NOTREACHED();
  return string16();
}

void BookmarkBubbleView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  HandleButtonPressed(sender);
}

void BookmarkBubbleView::LinkClicked(views::Link* source, int event_flags) {
  DCHECK(source == remove_link_);
  UserMetrics::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));

  // Set this so we remove the bookmark after the window closes.
  remove_bookmark_ = true;
  apply_edits_ = false;
  StartFade(false);
}

void BookmarkBubbleView::ItemChanged(views::Combobox* combo_box,
                                     int prev_index,
                                     int new_index) {
  if (new_index + 1 == parent_model_.GetItemCount()) {
    UserMetrics::RecordAction(
              UserMetricsAction("BookmarkBubble_EditFromCombobox"));
    ShowEditor();
  }
}

void BookmarkBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == edit_button_) {
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
    ShowEditor();
  } else {
    DCHECK_EQ(sender, close_button_);
    StartFade(false);
  }
}

void BookmarkBubbleView::ShowEditor() {
  const BookmarkNode* node =
      profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url_);
  views::Widget* parent = anchor_view()->GetWidget();
  Profile* profile = profile_;
  ApplyEdits();
  GetWidget()->Close();

  if (node)
    BookmarkEditor::Show(parent->GetNativeWindow(), profile,
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::SHOW_TREE);
}

void BookmarkBubbleView::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  BookmarkModel* model = profile_->GetBookmarkModel();
  const BookmarkNode* node = model->GetMostRecentlyAddedNodeForURL(url_);
  if (node) {
    const string16 new_title = title_tf_->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      UserMetrics::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }
    // Last index means 'Choose another folder...'
    if (parent_combobox_->selected_item() < parent_model_.GetItemCount() - 1) {
      const BookmarkNode* new_parent =
          parent_model_.GetNodeAt(parent_combobox_->selected_item());
      if (new_parent != node->parent()) {
        UserMetrics::RecordAction(
            UserMetricsAction("BookmarkBubble_ChangeParent"));
        model->Move(node, new_parent, new_parent->child_count());
      }
    }
  }
}
