// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/textfield_layout.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/sync/dice_bubble_sync_promo_view.h"
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

BookmarkBubbleView* BookmarkBubbleView::bookmark_bubble_ = nullptr;

namespace {

std::unique_ptr<views::View> CreateEditButton(views::ButtonListener* listener) {
  auto edit_button = views::MdTextButton::Create(
      listener, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS));
  edit_button->AddAccelerator(ui::Accelerator(ui::VKEY_E, ui::EF_ALT_DOWN));
  return edit_button;
}

}  // namespace

// static
views::Widget* BookmarkBubbleView::ShowBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    bookmarks::BookmarkBubbleObserver* observer,
    std::unique_ptr<BubbleSyncPromoDelegate> delegate,
    Profile* profile,
    const GURL& url,
    bool already_bookmarked) {
  if (bookmark_bubble_)
    return nullptr;

  bookmark_bubble_ =
      new BookmarkBubbleView(anchor_view, observer, std::move(delegate),
                             profile, url, !already_bookmarked);
  if (highlighted_button)
    bookmark_bubble_->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bookmark_bubble_);
  bubble_widget->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_bubble_->name_field_->SelectAll(true);

  if (bookmark_bubble_->observer_) {
    BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile);
    const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(url);
    bookmark_bubble_->observer_->OnBookmarkBubbleShown(node);
  }
  return bubble_widget;
}

// static
void BookmarkBubbleView::Hide() {
  if (bookmark_bubble_)
    bookmark_bubble_->GetWidget()->Close();
}

BookmarkBubbleView::~BookmarkBubbleView() {
  if (apply_edits_) {
    ApplyEdits();
  } else if (remove_bookmark_) {
    BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
    const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (node)
      model->Remove(node);
  }
}

// views::WidgetDelegate -------------------------------------------------------

views::View* BookmarkBubbleView::GetInitiallyFocusedView() {
  return name_field_;
}

void BookmarkBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK_EQ(bookmark_bubble_, this);
  bookmark_bubble_ = NULL;

  if (observer_)
    observer_->OnBookmarkBubbleHidden();
}

// views::DialogDelegate -------------------------------------------------------

void BookmarkBubbleView::OnDialogCancelled() {
  base::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
  // Set this so we remove the bookmark after the window closes.
  remove_bookmark_ = true;
  apply_edits_ = false;
}

void BookmarkBubbleView::OnDialogInitialized() {
  views::Button* cancel = GetCancelButton();
  if (cancel)
    cancel->AddAccelerator(ui::Accelerator(ui::VKEY_R, ui::EF_ALT_DOWN));
}

// views::ButtonListener -------------------------------------------------------

void BookmarkBubbleView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  base::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
  ShowEditor();
}

// views::ComboboxListener -----------------------------------------------------

void BookmarkBubbleView::OnPerformAction(views::Combobox* combobox) {
  if (combobox->GetSelectedIndex() + 1 == folder_model()->GetItemCount()) {
    base::RecordAction(UserMetricsAction("BookmarkBubble_EditFromCombobox"));
    ShowEditor();
  }
}

// views::BubbleDialogDelegateView ---------------------------------------------

void BookmarkBubbleView::Init() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  auto bookmark_contents_view = std::make_unique<views::View>();
  views::GridLayout* layout = bookmark_contents_view->SetLayoutManager(
      std::make_unique<views::GridLayout>());

  constexpr int kColumnId = 0;
  ConfigureTextfieldStack(layout, kColumnId);
  name_field_ = AddFirstTextfieldRow(
      layout, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_NAME_LABEL),
      kColumnId);
  name_field_->SetText(GetBookmarkName());
  name_field_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_NAME_LABEL));

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  auto parent_folder_model = std::make_unique<RecentlyUsedFoldersComboModel>(
      model, model->GetMostRecentlyAddedUserNodeForURL(url_));

  parent_combobox_ = AddComboboxRow(
      layout, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_LABEL),
      std::move(parent_folder_model), kColumnId);
  parent_combobox_->set_listener(this);
  parent_combobox_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_FOLDER_LABEL));

  bookmark_contents_view_ = AddChildView(std::move(bookmark_contents_view));
}

// Private methods -------------------------------------------------------------

BookmarkBubbleView::BookmarkBubbleView(
    views::View* anchor_view,
    bookmarks::BookmarkBubbleObserver* observer,
    std::unique_ptr<BubbleSyncPromoDelegate> delegate,
    Profile* profile,
    const GURL& url,
    bool newly_bookmarked)
    : LocationBarBubbleDelegateView(anchor_view, nullptr),
      observer_(observer),
      delegate_(std::move(delegate)),
      profile_(profile),
      url_(url) {
  DCHECK(anchor_view);

  WidgetDelegate::SetTitle(l10n_util::GetStringUTF16(
      newly_bookmarked ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED
                       : IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK));
  WidgetDelegate::SetShowCloseButton(true);

  SetArrow(views::BubbleBorder::TOP_RIGHT);
  SetButtonLabel(ui::DIALOG_BUTTON_OK, l10n_util::GetStringUTF16(IDS_DONE));
  SetButtonLabel(
      ui::DIALOG_BUTTON_CANCEL,
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK));
  SetExtraView(CreateEditButton(this));
  SetFootnoteView(CreateSigninPromoView());

  SetCancelCallback(base::BindOnce(&BookmarkBubbleView::OnDialogCancelled,
                                   base::Unretained(this)));

  chrome::RecordDialogCreation(chrome::DialogIdentifier::BOOKMARK);
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::CONTROL, views::CONTROL));
}

base::string16 BookmarkBubbleView::GetBookmarkName() {
  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const BookmarkNode* node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url_);
  if (node)
    return node->GetTitle();
  else
    NOTREACHED();
  return base::string16();
}

void BookmarkBubbleView::ShowEditor() {
  const BookmarkNode* node =
      BookmarkModelFactory::GetForBrowserContext(profile_)
          ->GetMostRecentlyAddedUserNodeForURL(url_);
  gfx::NativeWindow native_parent =
      anchor_widget() ? anchor_widget()->GetNativeWindow()
                      : platform_util::GetTopLevel(parent_window());
  DCHECK(native_parent);

  Profile* profile = profile_;
  ApplyEdits();
  GetWidget()->Close();

  if (node && native_parent)
    BookmarkEditor::Show(native_parent, profile,
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::SHOW_TREE);
}

void BookmarkBubbleView::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile_);
  const BookmarkNode* node = model->GetMostRecentlyAddedUserNodeForURL(url_);
  if (node) {
    const base::string16 new_title = name_field_->GetText();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      base::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }
    folder_model()->MaybeChangeParent(node,
                                      parent_combobox_->GetSelectedIndex());
  }
}

std::unique_ptr<views::View> BookmarkBubbleView::CreateSigninPromoView() {
#if defined(OS_CHROMEOS)
  // ChromeOS does not show the signin promo.
  return nullptr;
#else
  if (!SyncPromoUI::ShouldShowSyncPromo(profile_))
    return nullptr;

  return std::make_unique<DiceBubbleSyncPromoView>(
      profile_, delegate_.get(),
      signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
      IDS_BOOKMARK_DICE_PROMO_SYNC_MESSAGE,
      /*dice_signin_button_prominent=*/false);
#endif
}

BEGIN_METADATA(BookmarkBubbleView)
METADATA_PARENT_CLASS(LocationBarBubbleDelegateView);
END_METADATA()
