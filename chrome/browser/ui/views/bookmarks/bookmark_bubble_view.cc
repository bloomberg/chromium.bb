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
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/user_metrics.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/client_view.h"

using views::ColumnSet;
using views::GridLayout;

// Padding between "Title:" and the actual title.
static const int kTitlePadding = 4;

// Minimum width for the fields - they will push out the size of the bubble if
// necessary. This should be big enough so that the field pushes the right side
// of the bubble far enough so that the edit button's left edge is to the right
// of the field's left edge.
static const int kMinimumFieldSize = 180;

// Bubble close image.
static SkBitmap* kCloseImage = NULL;

// Declared in browser_dialogs.h so callers don't have to depend on our header.

namespace browser {

void ShowBookmarkBubbleView(views::Widget* parent,
                            const gfx::Rect& bounds,
                            BubbleDelegate* delegate,
                            Profile* profile,
                            const GURL& url,
                            bool newly_bookmarked) {
  BookmarkBubbleView::Show(parent, bounds, delegate, profile, url,
                           newly_bookmarked);
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
void BookmarkBubbleView::Show(views::Widget* parent,
                              const gfx::Rect& bounds,
                              BubbleDelegate* delegate,
                              Profile* profile,
                              const GURL& url,
                              bool newly_bookmarked) {
  if (IsShowing())
    return;

  bookmark_bubble_ = new BookmarkBubbleView(delegate, profile, url,
                                            newly_bookmarked);
  Bubble* bubble = Bubble::Show(
      parent, bounds, views::BubbleBorder::TOP_RIGHT, bookmark_bubble_,
      bookmark_bubble_);
  // |bubble_| can be set to NULL in BubbleClosing when we close the bubble
  // asynchronously. However, that can happen during the Show call above if the
  // window loses activation while we are getting to ready to show the bubble,
  // so we must check to make sure we still have a valid bubble before
  // proceeding.
  if (!bookmark_bubble_)
    return;
  bookmark_bubble_->set_bubble(bubble);
  bubble->SizeToContents();
  GURL url_ptr(url);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_BUBBLE_SHOWN,
      Source<Profile>(profile->GetOriginalProfile()),
      Details<GURL>(&url_ptr));
  bookmark_bubble_->BubbleShown();
}

// static
bool BookmarkBubbleView::IsShowing() {
  return bookmark_bubble_ != NULL;
}

void BookmarkBubbleView::Hide() {
  if (IsShowing())
    bookmark_bubble_->Close();
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

void BookmarkBubbleView::BubbleShown() {
  DCHECK(GetWidget());
  GetFocusManager()->RegisterAccelerator(
      views::Accelerator(ui::VKEY_RETURN, false, false, false), this);

  title_tf_->RequestFocus();
  title_tf_->SelectAll();
}

bool BookmarkBubbleView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  if (accelerator.key_code() != ui::VKEY_RETURN)
    return false;

  if (edit_button_->HasFocus())
    HandleButtonPressed(edit_button_);
  else
    HandleButtonPressed(close_button_);
  return true;
}

void BookmarkBubbleView::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  if (is_add && child == this)
    Init();
}

BookmarkBubbleView::BookmarkBubbleView(BubbleDelegate* delegate,
                                       Profile* profile,
                                       const GURL& url,
                                       bool newly_bookmarked)
    : delegate_(delegate),
      profile_(profile),
      url_(url),
      newly_bookmarked_(newly_bookmarked),
      parent_model_(
          profile_->GetBookmarkModel(),
          profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url)),
      remove_bookmark_(false),
      apply_edits_(true) {
}

void BookmarkBubbleView::Init() {
  static SkColor kTitleColor;
  static bool initialized = false;
  if (!initialized) {
    kTitleColor = color_utils::GetReadableColor(SkColorSetRGB(6, 45, 117),
                                                Bubble::kBackgroundColor);
    kCloseImage = ResourceBundle::GetSharedInstance().GetBitmapNamed(
      IDR_INFO_BUBBLE_CLOSE);

    initialized = true;
  }

  remove_link_ = new views::Link(UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK)));
  remove_link_->set_listener(this);

  edit_button_ = new views::NativeTextButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS)));

  close_button_ = new views::NativeTextButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_DONE)));
  close_button_->SetIsDefault(true);

  views::Label* combobox_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_TEXT)));

  parent_combobox_ = new views::Combobox(&parent_model_);
  parent_combobox_->SetSelectedItem(parent_model_.node_parent_index());
  parent_combobox_->set_listener(this);
  parent_combobox_->SetAccessibleName(
      WideToUTF16Hack(combobox_label->GetText()));

  views::Label* title_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED :
                              IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK)));
  title_label->SetFont(
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  title_label->SetColor(kTitleColor);

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
  layout->AddView(new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_TITLE_TEXT))));
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

  bubble_->set_fade_away_on_close(true);
  Close();
}

void BookmarkBubbleView::ItemChanged(views::Combobox* combobox,
                                     int prev_index,
                                     int new_index) {
  if (new_index + 1 == parent_model_.GetItemCount()) {
    UserMetrics::RecordAction(
              UserMetricsAction("BookmarkBubble_EditFromCombobox"));

    ShowEditor();
    return;
  }
}

void BookmarkBubbleView::BubbleClosing(Bubble* bubble,
                                       bool closed_by_escape) {
  if (closed_by_escape) {
    remove_bookmark_ = newly_bookmarked_;
    apply_edits_ = false;
  }

  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK(bookmark_bubble_ == this);
  bookmark_bubble_ = NULL;

  if (delegate_)
    delegate_->BubbleClosing(bubble, closed_by_escape);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_BUBBLE_HIDDEN,
      Source<Profile>(profile_->GetOriginalProfile()),
      NotificationService::NoDetails());
}

bool BookmarkBubbleView::CloseOnEscape() {
  return delegate_ ? delegate_->CloseOnEscape() : true;
}

bool BookmarkBubbleView::FadeInOnShow() {
  return false;
}

std::wstring BookmarkBubbleView::accessible_name() {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_ADD_BOOKMARK));
}

void BookmarkBubbleView::Close() {
  ApplyEdits();
  GetWidget()->Close();
}

void BookmarkBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == edit_button_) {
    UserMetrics::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
    bubble_->set_fade_away_on_close(true);
    ShowEditor();
  } else {
    DCHECK(sender == close_button_);
    bubble_->set_fade_away_on_close(true);
    Close();
  }
  // WARNING: we've most likely been deleted when CloseWindow returns.
}

void BookmarkBubbleView::ShowEditor() {
  const BookmarkNode* node =
      profile_->GetBookmarkModel()->GetMostRecentlyAddedNodeForURL(url_);

#if !defined(WEBUI_DIALOGS)
#if defined(OS_WIN)
  // Parent the editor to our root ancestor (not the root we're in, as that
  // is the info bubble and will close shortly).
  HWND parent = GetAncestor(GetWidget()->GetNativeView(), GA_ROOTOWNER);

  // We're about to show the bookmark editor. When the bookmark editor closes
  // we want the browser to become active. NativeWidgetWin::Hide() does a hide
  // in a such way that activation isn't changed, which means when we close
  // Windows gets confused as to who it should give active status to. We
  // explicitly hide the bookmark bubble window in such a way that activation
  // status changes. That way, when the editor closes, activation is properly
  // restored to the browser.
  ShowWindow(GetWidget()->GetNativeView(), SW_HIDE);
#elif defined(TOOLKIT_USES_GTK)
  gfx::NativeWindow parent = GTK_WINDOW(
      static_cast<views::NativeWidgetGtk*>(GetWidget()->native_widget())->
          GetTransientParent());
#endif
#endif

  // Even though we just hid the window, we need to invoke Close to schedule
  // the delete and all that.
  Close();

  if (node) {
#if defined(WEBUI_DIALOGS)
    Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
    DCHECK(browser);
    browser->OpenBookmarkManagerEditNode(node->id());
#else
    BookmarkEditor::Show(parent, profile_, NULL,
                         BookmarkEditor::EditDetails(node),
                         BookmarkEditor::SHOW_TREE);
#endif
  }
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
    if (parent_combobox_->selected_item() <
        parent_model_.GetItemCount() - 1) {
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
