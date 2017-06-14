// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_observer.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

#if defined(OS_WIN)
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_bubble_view.h"
#include "chrome/browser/ui/views/desktop_ios_promotion/desktop_ios_promotion_footnote_view.h"
#include "components/browser_sync/profile_sync_service.h"
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// This combobox prevents any lengthy content from stretching the bubble view.
class UnsizedCombobox : public views::Combobox {
 public:
  explicit UnsizedCombobox(ui::ComboboxModel* model) : views::Combobox(model) {}
  ~UnsizedCombobox() override {}

  // views::Combobox:
  gfx::Size CalculatePreferredSize() const override {
    return gfx::Size(0, views::Combobox::CalculatePreferredSize().height());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UnsizedCombobox);
};

}  // namespace

BookmarkBubbleView* BookmarkBubbleView::bookmark_bubble_ = nullptr;

// static
views::Widget* BookmarkBubbleView::ShowBubble(
    views::View* anchor_view,
    const gfx::Rect& anchor_rect,
    gfx::NativeView parent_window,
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
  // Bookmark bubble should always anchor TOP_RIGHT, but the
  // LocationBarBubbleDelegateView does not know that and may use different
  // arrow anchoring.
  bookmark_bubble_->set_arrow(views::BubbleBorder::TOP_RIGHT);
  if (!anchor_view) {
    bookmark_bubble_->SetAnchorRect(anchor_rect);
    bookmark_bubble_->set_parent_window(parent_window);
  }
  views::Widget* bubble_widget =
      views::BubbleDialogDelegateView::CreateBubble(bookmark_bubble_);
  bubble_widget->Show();
  // Select the entire title textfield contents when the bubble is first shown.
  bookmark_bubble_->name_field_->SelectAll(true);
  bookmark_bubble_->SetArrowPaintType(views::BubbleBorder::PAINT_TRANSPARENT);

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
  // |parent_combobox_| needs to be destroyed before |parent_model_| as it
  // uses |parent_model_| in its destructor.
  delete parent_combobox_;
}

// ui::DialogModel -------------------------------------------------------------

int BookmarkBubbleView::GetDialogButtons() const {
  // TODO(tapted): DialogClientView should manage the ios promo buttons too.
  return is_showing_ios_promotion_
             ? ui::DIALOG_BUTTON_NONE
             : (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL);
}

base::string16 BookmarkBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK)
                                       ? IDS_DONE
                                       : IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK);
}

// views::WidgetDelegate -------------------------------------------------------

views::View* BookmarkBubbleView::GetInitiallyFocusedView() {
  return name_field_;
}

base::string16 BookmarkBubbleView::GetWindowTitle() const {
#if defined(OS_WIN)
  if (is_showing_ios_promotion_) {
    return desktop_ios_promotion::GetPromoTitle(
        desktop_ios_promotion::PromotionEntryPoint::BOOKMARKS_BUBBLE);
  }
#endif
  return l10n_util::GetStringUTF16(newly_bookmarked_
                                       ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED
                                       : IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK);
}

gfx::ImageSkia BookmarkBubbleView::GetWindowIcon() {
#if defined(OS_WIN)
  if (is_showing_ios_promotion_) {
    return desktop_ios_promotion::GetPromoImage(
        GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_TextfieldDefaultColor));
  }
#endif
  return gfx::ImageSkia();
}

bool BookmarkBubbleView::ShouldShowWindowIcon() const {
  return is_showing_ios_promotion_;
}

void BookmarkBubbleView::WindowClosing() {
  // We have to reset |bubble_| here, not in our destructor, because we'll be
  // destroyed asynchronously and the shown state will be checked before then.
  DCHECK_EQ(bookmark_bubble_, this);
  bookmark_bubble_ = NULL;
  is_showing_ios_promotion_ = false;

  if (observer_)
    observer_->OnBookmarkBubbleHidden();
}

// views::DialogDelegate -------------------------------------------------------

views::View* BookmarkBubbleView::CreateExtraView() {
  edit_button_ = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS));
  edit_button_->AddAccelerator(ui::Accelerator(ui::VKEY_E, ui::EF_ALT_DOWN));
  return edit_button_;
}

bool BookmarkBubbleView::GetExtraViewPadding(int* padding) {
  *padding = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_UNRELATED_CONTROL_HORIZONTAL_LARGE);
  return true;
}

views::View* BookmarkBubbleView::CreateFootnoteView() {
#if defined(OS_WIN)
  if (!is_showing_ios_promotion_ &&
      IsIOSPromotionEligible(
          desktop_ios_promotion::PromotionEntryPoint::BOOKMARKS_FOOTNOTE)) {
    footnote_view_ = new DesktopIOSPromotionFootnoteView(profile_, this);
    return footnote_view_;
  }
#endif
  if (!SyncPromoUI::ShouldShowSyncPromo(profile_))
    return nullptr;

  base::RecordAction(UserMetricsAction("Signin_Impression_FromBookmarkBubble"));

  footnote_view_ =
      new BubbleSyncPromoView(delegate_.get(), IDS_BOOKMARK_SYNC_PROMO_LINK,
                              IDS_BOOKMARK_SYNC_PROMO_MESSAGE);
  return footnote_view_;
}

bool BookmarkBubbleView::Cancel() {
  base::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
  // Set this so we remove the bookmark after the window closes.
  remove_bookmark_ = true;
  apply_edits_ = false;
  return true;
}

bool BookmarkBubbleView::Accept() {
#if defined(OS_WIN)
  using desktop_ios_promotion::PromotionEntryPoint;
  if (IsIOSPromotionEligible(PromotionEntryPoint::BOOKMARKS_BUBBLE)) {
    ShowIOSPromotion(PromotionEntryPoint::BOOKMARKS_BUBBLE);
    return false;
  }
#endif
  return true;
}

bool BookmarkBubbleView::Close() {
  // Allow closing when activation lost. Default would call Accept().
  return true;
}

void BookmarkBubbleView::UpdateButton(views::LabelButton* button,
                                      ui::DialogButton type) {
  LocationBarBubbleDelegateView::UpdateButton(button, type);
  if (type == ui::DIALOG_BUTTON_CANCEL)
    button->AddAccelerator(ui::Accelerator(ui::VKEY_R, ui::EF_ALT_DOWN));
}

// views::View -----------------------------------------------------------------

const char* BookmarkBubbleView::GetClassName() const {
  return "BookmarkBubbleView";
}

void BookmarkBubbleView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  LocationBarBubbleDelegateView::GetAccessibleNodeData(node_data);
  node_data->SetName(l10n_util::GetStringUTF8(
      newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED
                        : IDS_BOOKMARK_AX_BUBBLE_PAGE_BOOKMARK));
}

// views::ButtonListener -------------------------------------------------------

void BookmarkBubbleView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  base::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
  ShowEditor();
}

// views::ComboboxListener -----------------------------------------------------

void BookmarkBubbleView::OnPerformAction(views::Combobox* combobox) {
  if (combobox->selected_index() + 1 == parent_model_.GetItemCount()) {
    base::RecordAction(UserMetricsAction("BookmarkBubble_EditFromCombobox"));
    ShowEditor();
  }
}

// DesktopIOSPromotionFootnoteDelegate -----------------------------------------

void BookmarkBubbleView::OnIOSPromotionFootnoteLinkClicked() {
#if defined(OS_WIN)
  ShowIOSPromotion(
      desktop_ios_promotion::PromotionEntryPoint::FOOTNOTE_FOLLOWUP_BUBBLE);
#endif
}

// views::BubbleDialogDelegateView ---------------------------------------------

void BookmarkBubbleView::Init() {
  using views::GridLayout;

  SetLayoutManager(new views::FillLayout());
  bookmark_contents_view_ = new views::View();
  GridLayout* layout = new GridLayout(bookmark_contents_view_);
  bookmark_contents_view_->SetLayoutManager(layout);

  // This column set is used for the labels and textfields.
  constexpr int kColumnId = 0;
  constexpr float kFixed = 0.f;
  constexpr float kStretchy = 1.f;
  views::ColumnSet* cs = layout->AddColumnSet(kColumnId);
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  cs->AddColumn(provider->GetControlLabelGridAlignment(), GridLayout::CENTER,
                kFixed, GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(kFixed, provider->GetDistanceMetric(
                                   DISTANCE_UNRELATED_CONTROL_HORIZONTAL));
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, kStretchy,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(kFixed, kColumnId);
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_NAME_LABEL));
  layout->AddView(label);

  name_field_ = new views::Textfield();
  name_field_->SetText(GetBookmarkName());
  name_field_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_NAME_LABEL));
  layout->AddView(name_field_);

  layout->StartRowWithPadding(
      kFixed, kColumnId, kFixed,
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL));
  views::Label* combobox_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_LABEL));
  layout->AddView(combobox_label);

  parent_combobox_ = new UnsizedCombobox(&parent_model_);
  parent_combobox_->set_listener(this);
  parent_combobox_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_FOLDER_LABEL));
  layout->AddView(parent_combobox_);

  if (provider->UseExtraDialogPadding()) {
    layout->AddPaddingRow(
        kFixed,
        provider->GetInsetsMetric(views::INSETS_DIALOG_CONTENTS).bottom());
  }

  AddChildView(bookmark_contents_view_);
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
      url_(url),
      newly_bookmarked_(newly_bookmarked),
      parent_model_(BookmarkModelFactory::GetForBrowserContext(profile_),
                    BookmarkModelFactory::GetForBrowserContext(profile_)
                        ->GetMostRecentlyAddedUserNodeForURL(url)) {
  chrome::RecordDialogCreation(chrome::DialogIdentifier::BOOKMARK);
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
    const base::string16 new_title = name_field_->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title);
      base::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }
    parent_model_.MaybeChangeParent(node, parent_combobox_->selected_index());
  }
}

#if defined(OS_WIN)

bool BookmarkBubbleView::IsIOSPromotionEligible(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  PrefService* prefs = profile_->GetPrefs();
  const browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return desktop_ios_promotion::IsEligibleForIOSPromotion(prefs, sync_service,
                                                          entry_point);
}

void BookmarkBubbleView::ShowIOSPromotion(
    desktop_ios_promotion::PromotionEntryPoint entry_point) {
  DCHECK(!is_showing_ios_promotion_);
  edit_button_->SetVisible(false);
  // Hide the contents, but don't delete. Its child views are accessed in the
  // destructor if there are edits to apply.
  bookmark_contents_view_->SetVisible(false);
  delete footnote_view_;
  footnote_view_ = nullptr;
  is_showing_ios_promotion_ = true;
  ios_promo_view_ = new DesktopIOSPromotionBubbleView(profile_, entry_point);
  AddChildView(ios_promo_view_);
  GetWidget()->UpdateWindowIcon();
  GetWidget()->UpdateWindowTitle();
  GetDialogClientView()->UpdateDialogButtons();
  SizeToContents();
}
#endif
