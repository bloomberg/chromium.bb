// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webauthn/hover_list_view.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/vector_icons.h"

namespace {

constexpr int kPlaceHolderItemTag = -1;

std::unique_ptr<HoverButton> CreateHoverButtonForListItem(
    int item_tag,
    const gfx::VectorIcon& vector_icon,
    base::string16 item_title,
    base::string16 item_description,
    views::ButtonListener* listener,
    bool is_placeholder_item = false) {
  // TODO(hongjunchoi): Make HoverListView subclass of HoverButton and listen
  // for potential native theme color change.
  //
  // Derive the icon color from the text color of an enabled label.
  auto color_reference_label = std::make_unique<views::Label>(
      base::string16(), CONTEXT_BODY_TEXT_SMALL, views::style::STYLE_PRIMARY);
  const SkColor icon_color = color_utils::DeriveDefaultIconColor(
      color_reference_label->enabled_color());

  constexpr int kIconSize = 20;
  auto item_image = std::make_unique<views::ImageView>();
  item_image->SetImage(
      gfx::CreateVectorIcon(vector_icon, kIconSize, icon_color));

  constexpr int kChevronSize = 8;
  constexpr int kChevronPadding = (kIconSize - kChevronSize) / 2;
  std::unique_ptr<views::ImageView> chevron_image = nullptr;
  if (!is_placeholder_item) {
    chevron_image = std::make_unique<views::ImageView>();
    chevron_image->SetImage(gfx::CreateVectorIcon(views::kSubmenuArrowIcon,
                                                  kChevronSize, icon_color));
    chevron_image->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kChevronPadding)));
  }

  auto hover_button = std::make_unique<HoverButton>(
      listener, std::move(item_image), std::move(item_title),
      std::move(item_description), std::move(chevron_image));
  hover_button->set_tag(item_tag);

  // Because there is an icon on both sides, set a custom border that has only
  // half of the normal padding horizontally.
  constexpr int kExtraVerticalPadding = 2;
  constexpr int kHorizontalPadding = 8;
  gfx::Insets padding(views::LayoutProvider::Get()->GetDistanceMetric(
                          DISTANCE_CONTROL_LIST_VERTICAL) +
                          kExtraVerticalPadding,
                      kHorizontalPadding);
  hover_button->SetBorder(views::CreateEmptyBorder(padding));

  if (is_placeholder_item) {
    hover_button->SetState(HoverButton::ButtonState::STATE_DISABLED);
    const auto background_color =
        hover_button->GetNativeTheme()->GetSystemColor(
            ui::NativeTheme::kColorId_BubbleBackground);
    hover_button->SetTitleTextStyle(views::style::STYLE_DISABLED,
                                    background_color);
  }

  return hover_button;
}

views::Separator* AddSeparatorAsChild(views::View* view) {
  auto* separator = new views::Separator();
  separator->SetColor(gfx::kGoogleGrey300);
  view->AddChildView(separator);
  return separator;
}

}  // namespace

// HoverListView ---------------------------------------------------------

HoverListView::HoverListView(std::unique_ptr<HoverListModel> model)
    : model_(std::move(model)) {
  DCHECK(model_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto item_container = std::make_unique<views::View>();
  item_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(),
      0 /* betweeen_child_spacing */));

  item_container_ = item_container.get();
  AddSeparatorAsChild(item_container_);

  for (const auto item_tag : model_->GetItemTags()) {
    AppendListItemView(model_->GetItemIcon(item_tag),
                       model_->GetItemText(item_tag),
                       model_->GetDescriptionText(item_tag), item_tag);
  }

  if (tags_to_list_item_views_.empty() &&
      model_->ShouldShowPlaceholderForEmptyList()) {
    CreateAndAppendPlaceholderItem();
  }

  scroll_view_ = new views::ScrollView();
  scroll_view_->SetContents(std::move(item_container));
  AddChildView(scroll_view_);
  scroll_view_->ClipHeightTo(GetPreferredViewHeight(),
                             GetPreferredViewHeight());

  model_->SetObserver(this);
}

HoverListView::~HoverListView() {
  model_->RemoveObserver();
}

void HoverListView::AppendListItemView(const gfx::VectorIcon& icon,
                                       base::string16 item_text,
                                       base::string16 description_text,
                                       int item_tag) {
  auto hover_button = CreateHoverButtonForListItem(item_tag, icon, item_text,
                                                   description_text, this);

  auto* list_item_view_ptr = hover_button.release();
  item_container_->AddChildView(list_item_view_ptr);
  auto* separator = AddSeparatorAsChild(item_container_);
  tags_to_list_item_views_.emplace(
      item_tag, ListItemViews{list_item_view_ptr, separator});
}

void HoverListView::CreateAndAppendPlaceholderItem() {
  auto placeholder_item = CreateHoverButtonForListItem(
      kPlaceHolderItemTag, model_->GetPlaceholderIcon(),
      model_->GetPlaceholderText(), base::string16(), nullptr,
      true /* is_placeholder_item */);
  item_container_->AddChildView(placeholder_item.get());
  auto* separator = AddSeparatorAsChild(item_container_);
  placeholder_list_item_view_.emplace(
      ListItemViews{placeholder_item.release(), separator});
}

void HoverListView::AddListItemView(int item_tag) {
  CHECK(!base::ContainsKey(tags_to_list_item_views_, item_tag));
  if (placeholder_list_item_view_) {
    RemoveListItemView(*placeholder_list_item_view_);
    placeholder_list_item_view_.reset();
  }

  AppendListItemView(model_->GetItemIcon(item_tag),
                     model_->GetItemText(item_tag),
                     model_->GetDescriptionText(item_tag), item_tag);

  // TODO(hongjunchoi): The enclosing dialog may also need to be resized,
  // similarly to what is done in
  // AuthenticatorRequestDialogView::ReplaceSheetWith().
  Layout();
}

void HoverListView::RemoveListItemView(int item_tag) {
  auto view_it = tags_to_list_item_views_.find(item_tag);
  if (view_it == tags_to_list_item_views_.end())
    return;

  RemoveListItemView(view_it->second);
  tags_to_list_item_views_.erase(view_it);

  // Removed list item may have not been the bottom-most view in the scroll
  // view. To enforce that all remaining items are re-shifted to the top,
  // invalidate all child views.
  //
  // TODO(hongjunchoi): Restructure HoverListView and |scroll_view_| so that
  // InvalidateLayout() does not need to be explicitly called when items are
  // removed from the list. See: https://crbug.com/904968
  item_container_->InvalidateLayout();

  if (tags_to_list_item_views_.empty() &&
      model_->ShouldShowPlaceholderForEmptyList()) {
    CreateAndAppendPlaceholderItem();
  }

  // TODO(hongjunchoi): The enclosing dialog may also need to be resized,
  // similarly to what is done in
  // AuthenticatorRequestDialogView::ReplaceSheetWith().
  Layout();
}

void HoverListView::RemoveListItemView(ListItemViews list_item) {
  DCHECK(item_container_->Contains(list_item.item_view));
  DCHECK(item_container_->Contains(list_item.separator_view));
  item_container_->RemoveChildView(list_item.item_view);
  item_container_->RemoveChildView(list_item.separator_view);
}

views::Button& HoverListView::GetTopListItemView() const {
  DCHECK(!tags_to_list_item_views_.empty());
  return *tags_to_list_item_views_.begin()->second.item_view;
}

void HoverListView::RequestFocus() {
  if (tags_to_list_item_views_.empty())
    return;

  GetTopListItemView().RequestFocus();
}

void HoverListView::OnListItemAdded(int item_tag) {
  AddListItemView(item_tag);
}

void HoverListView::OnListItemRemoved(int removed_item_tag) {
  RemoveListItemView(removed_item_tag);
}

void HoverListView::OnListItemChanged(int changed_list_item_tag,
                                      HoverListModel::ListItemChangeType type) {
  if (type == HoverListModel::ListItemChangeType::kAddToViewComponent) {
    AddListItemView(changed_list_item_tag);
  } else {
    RemoveListItemView(changed_list_item_tag);
  }
}

void HoverListView::ButtonPressed(views::Button* sender,
                                  const ui::Event& event) {
  model_->OnListItemSelected(sender->tag());
}

int HoverListView::GetPreferredViewHeight() const {
  constexpr int kMaxViewHeight = 300;

  // |item_container_| has one separator at the top and list items which
  // contain one separator and one hover button.
  const auto separator_height = views::Separator().GetPreferredSize().height();
  int size = separator_height;
  for (auto iter = tags_to_list_item_views_.begin();
       iter != tags_to_list_item_views_.end(); ++iter) {
    size +=
        iter->second.item_view->GetPreferredSize().height() + separator_height;
  }
  int reserved_items =
      model_->GetPreferredItemCount() - tags_to_list_item_views_.size();
  if (reserved_items > 0) {
    auto dummy_hover_button = CreateHoverButtonForListItem(
        -1 /* tag */, gfx::kNoneIcon, base::string16(), base::string16(),
        nullptr /* listener */);
    const auto list_item_height =
        separator_height + dummy_hover_button->GetPreferredSize().height();
    size += list_item_height * reserved_items;
  }
  return std::min(kMaxViewHeight, size);
}
