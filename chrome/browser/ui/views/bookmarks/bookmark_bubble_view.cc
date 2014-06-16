// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_sync_promo_view.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;
using views::ColumnSet;
using views::GridLayout;

namespace {

// Width of the border of a button.
const int kControlBorderWidth = 2;

// This combobox prevents any lengthy content from stretching the bubble view.
class UnsizedCombobox : public views::Combobox {
 public:
  explicit UnsizedCombobox(ui::ComboboxModel* model) : views::Combobox(model) {}
  virtual ~UnsizedCombobox() {}

  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(0, views::Combobox::GetPreferredSize().height());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnsizedCombobox);
};

}  // namespace

BookmarkBubbleView* BookmarkBubbleView::bookmark_bubble_ = NULL;

// static
void BookmarkBubbleView::ShowBubble(views::View* anchor_view,
                                    BookmarkBubbleViewObserver* observer,
                                    scoped_ptr<BookmarkBubbleDelegate> delegate,
                                    Profile* profile,
                                    const GURL& url,
                                    bool newly_bookmarked) {
  if (IsShowing())
    return;

  bookmark_bubble_ = new BookmarkBubbleView(anchor_view,
                                            observer,
                                            delegate.Pass(),
                                            profile,
                                            url,
                                            newly_bookmarked);
  views::BubbleDelegateView::CreateBubble(bookmark_bubble_)->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_bubble_->title_tf_->SelectAll(true);
  bookmark_bubble_->SetArrowPaintType(views::BubbleBorder::PAINT_NONE);

  if (bookmark_bubble_->observer_)
    bookmark_bubble_->observer_->OnBookmarkBubbleShown(url);
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
    BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
    const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (node)
      model->Remove(node->parent(), node->parent()->GetIndexOf(node));
  }
  // |parent_combobox_| needs to be destroyed before |parent_model_| as it
  // uses |parent_model_| in its destructor.
  delete parent_combobox_;
}

views::View* BookmarkBubbleView::GetInitiallyFocusedView() {
  return title_tf_;
}

void BookmarkBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK_EQ(bookmark_bubble_, this);
  bookmark_bubble_ = NULL;

  if (observer_)
    observer_->OnBookmarkBubbleHidden();
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
  views::Label* title_label = new views::Label(
      l10n_util::GetStringUTF16(
          newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED :
                              IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK));
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::MediumFont));

  remove_button_ = new views::LabelButton(this, l10n_util::GetStringUTF16(
      IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK));
  remove_button_->SetStyle(views::Button::STYLE_BUTTON);

  edit_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS));
  edit_button_->SetStyle(views::Button::STYLE_BUTTON);

  close_button_ = new views::LabelButton(
      this, l10n_util::GetStringUTF16(IDS_DONE));
  close_button_->SetStyle(views::Button::STYLE_BUTTON);
  close_button_->SetIsDefault(true);

  views::Label* combobox_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_TEXT));

  parent_combobox_ = new UnsizedCombobox(&parent_model_);
  parent_combobox_->set_listener(this);
  parent_combobox_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_FOLDER_TEXT));

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Column sets used in the layout of the bubble.
  enum ColumnSetID {
    TITLE_COLUMN_SET_ID,
    CONTENT_COLUMN_SET_ID,
    SYNC_PROMO_COLUMN_SET_ID
  };

  ColumnSet* cs = layout->AddColumnSet(TITLE_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0, GridLayout::USE_PREF,
                0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  // The column layout used for middle and bottom rows.
  cs = layout->AddColumnSet(CONTENT_COLUMN_SET_ID);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);
  cs->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);

  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlLargeHorizontalSpacing);

  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(GridLayout::LEADING, GridLayout::TRAILING, 0,
                GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kButtonHEdgeMarginNew);

  layout->StartRow(0, TITLE_COLUMN_SET_ID);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlHorizontalSpacing);

  layout->StartRow(0, CONTENT_COLUMN_SET_ID);
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_TITLE_TEXT));
  layout->AddView(label);
  title_tf_ = new views::Textfield();
  title_tf_->SetText(GetTitle());
  title_tf_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_TITLE_TEXT));

  layout->AddView(title_tf_, 5, 1);

  layout->AddPaddingRow(0, views::kUnrelatedControlHorizontalSpacing);

  layout->StartRow(0, CONTENT_COLUMN_SET_ID);
  layout->AddView(combobox_label);
  layout->AddView(parent_combobox_, 5, 1);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  layout->StartRow(0, CONTENT_COLUMN_SET_ID);
  layout->SkipColumns(2);
  layout->AddView(remove_button_);
  layout->AddView(edit_button_);
  layout->AddView(close_button_);

  layout->AddPaddingRow(
      0,
      views::kUnrelatedControlVerticalSpacing - kControlBorderWidth);

  if (SyncPromoUI::ShouldShowSyncPromo(profile_)) {
    // The column layout used for the sync promo.
    cs = layout->AddColumnSet(SYNC_PROMO_COLUMN_SET_ID);
    cs->AddColumn(GridLayout::FILL,
                  GridLayout::FILL,
                  1,
                  GridLayout::USE_PREF,
                  0,
                  0);
    layout->StartRow(0, SYNC_PROMO_COLUMN_SET_ID);

    sync_promo_view_ = new BookmarkSyncPromoView(delegate_.get());
    layout->AddView(sync_promo_view_);
  }

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, ui::EF_NONE));
}

BookmarkBubbleView::BookmarkBubbleView(
    views::View* anchor_view,
    BookmarkBubbleViewObserver* observer,
    scoped_ptr<BookmarkBubbleDelegate> delegate,
    Profile* profile,
    const GURL& url,
    bool newly_bookmarked)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      observer_(observer),
      delegate_(delegate.Pass()),
      profile_(profile),
      url_(url),
      newly_bookmarked_(newly_bookmarked),
      parent_model_(
          BookmarkModelFactory::GetForProfile(profile_),
          BookmarkModelFactory::GetForProfile(profile_)->
              GetMostRecentlyAddedUserNodeForURL(url)),
      remove_button_(NULL),
      edit_button_(NULL),
      close_button_(NULL),
      title_tf_(NULL),
      parent_combobox_(NULL),
      sync_promo_view_(NULL),
      remove_bookmark_(false),
      apply_edits_(true) {
  set_margins(gfx::Insets(views::kPanelVertMargin, 0, 0, 0));
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(2, 0, 2, 0));
}

base::string16 BookmarkBubbleView::GetTitle() {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(profile_);
  const BookmarkNode* node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url_);
  if (node)
    return node->GetTitle();
  else
    NOTREACHED();
  return base::string16();
}

void BookmarkBubbleView::GetAccessibleState(ui::AXViewState* state) {
  BubbleDelegateView::GetAccessibleState(state);
  state->name =
      l10n_util::GetStringUTF16(
          newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED :
                              IDS_BOOKMARK_AX_BUBBLE_PAGE_BOOKMARK);
}

void BookmarkBubbleView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  HandleButtonPressed(sender);
}

void BookmarkBubbleView::OnPerformAction(views::Combobox* combobox) {
  if (combobox->selected_index() + 1 == parent_model_.GetItemCount()) {
    content::RecordAction(UserMetricsAction("BookmarkBubble_EditFromCombobox"));
    ShowEditor();
  }
}

void BookmarkBubbleView::HandleButtonPressed(views::Button* sender) {
  if (sender == remove_button_) {
    content::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
    // Set this so we remove the bookmark after the window closes.
    remove_bookmark_ = true;
    apply_edits_ = false;
    GetWidget()->Close();
  } else if (sender == edit_button_) {
    content::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
    ShowEditor();
  } else {
    DCHECK_EQ(close_button_, sender);
    GetWidget()->Close();
  }
}

void BookmarkBubbleView::ShowEditor() {
  const BookmarkNode* node = BookmarkModelFactory::GetForProfile(
      profile_)->GetMostRecentlyAddedUserNodeForURL(url_);
  views::Widget* parent = anchor_widget();
  DCHECK(parent);

  Profile* profile = profile_;
  ApplyEdits();
  GetWidget()->Close();

  if (node && parent)
    BookmarkEditor::Show(parent->GetNativeWindow(), profile,
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::SHOW_TREE);
}

void BookmarkBubbleView::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile_);
  const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(url_);
  if (node) {
    const base::string16 new_title = title_tf_->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      content::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }
    parent_model_.MaybeChangeParent(node, parent_combobox_->selected_index());
  }
}
