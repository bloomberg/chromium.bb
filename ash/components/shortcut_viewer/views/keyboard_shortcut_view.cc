// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"

#include <algorithm>

#include "ash/components/resources/grit/ash_components_resources.h"
#include "ash/components/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/components/shortcut_viewer/vector_icons/vector_icons.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_list_view.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_view.h"
#include "ash/components/shortcut_viewer/views/ksv_search_box_view.h"
#include "ash/components/strings/grit/ash_components_strings.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/window_properties.h"
#include "base/i18n/string_search.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/default_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/search_box/search_box_view_base.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

KeyboardShortcutView* g_ksv_view = nullptr;

// Setups the illustration views for search states, including an icon and a
// descriptive text.
void SetupSearchIllustrationView(views::View* illustration_view,
                                 const gfx::VectorIcon& icon,
                                 int message_id) {
  constexpr int kSearchIllustrationIconSize = 150;
  constexpr SkColor kSearchIllustrationIconColor =
      SkColorSetARGBMacro(0xFF, 0xDA, 0xDC, 0xE0);

  illustration_view->set_owned_by_client();
  constexpr int kTopPadding = 98;
  views::BoxLayout* layout =
      illustration_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kVertical, gfx::Insets(kTopPadding, 0, 0, 0)));
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  views::ImageView* image_view = new views::ImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(icon, kSearchIllustrationIconColor));
  image_view->SetImageSize(
      gfx::Size(kSearchIllustrationIconSize, kSearchIllustrationIconSize));
  illustration_view->AddChildView(image_view);

  constexpr SkColor kSearchIllustrationTextColor =
      SkColorSetARGBMacro(0xFF, 0x20, 0x21, 0x24);
  views::Label* text = new views::Label(l10n_util::GetStringUTF16(message_id));
  text->SetEnabledColor(kSearchIllustrationTextColor);
  constexpr int kLabelFontSizeDelta = 1;
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  text->SetFontList(rb.GetFontListWithDelta(
      kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  illustration_view->AddChildView(text);
}

views::ScrollView* CreateScrollView() {
  views::ScrollView* const scroller = new views::ScrollView();
  scroller->set_draw_overflow_indicator(false);
  scroller->ClipHeightTo(0, 0);
  return scroller;
}

}  // namespace

KeyboardShortcutView::~KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, this);
  g_ksv_view = nullptr;
}

// static
views::Widget* KeyboardShortcutView::Show(gfx::NativeWindow context) {
  if (g_ksv_view) {
    // If there is a KeyboardShortcutView window open already, just activate
    // it.
    g_ksv_view->GetWidget()->Activate();
  } else {
    constexpr gfx::Size kKSVWindowSize(768, 512);
    gfx::Rect window_bounds(kKSVWindowSize);
    if (context) {
      window_bounds = context->GetRootWindow()->bounds();
      window_bounds.ClampToCenteredSize(kKSVWindowSize);
    }
    views::Widget::CreateWindowWithContextAndBounds(new KeyboardShortcutView(),
                                                    context, window_bounds);

    // Set frame view Active and Inactive colors, both are SK_ColorWHITE.
    aura::Window* window = g_ksv_view->GetWidget()->GetNativeWindow();
    window->SetProperty(ash::kFrameActiveColorKey, SK_ColorWHITE);
    window->SetProperty(ash::kFrameInactiveColorKey, SK_ColorWHITE);

    // Set shelf icon.
    // An app id for keyboard shortcut viewer window, also used to identify the
    // shelf item. Generated as
    // crx_file::id_util::GenerateId("org.chromium.keyboardshortcutviewer")
    static constexpr char kKeyboardShortcutViewerId[] =
        "dieikdblbimmfmfinbibdlalidbnbchd";
    const ash::ShelfID shelf_id(kKeyboardShortcutViewerId);
    window->SetProperty(ash::kShelfIDKey,
                        new std::string(shelf_id.Serialize()));
    window->SetProperty<int>(ash::kShelfItemTypeKey, ash::TYPE_DIALOG);

    // We don't want the KSV window to have a title (per design), however the
    // shelf uses the window title to set the shelf item's tooltip text. The
    // shelf observes changes to the |kWindowIconKey| property and handles that
    // by initializing the shelf item including its tooltip text.
    window->SetTitle(l10n_util::GetStringUTF16(IDS_KSV_TITLE));
    gfx::ImageSkia* icon =
        ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_KEYBOARD_SHORTCUT_VIEWER_APP_ICON);
    // The new gfx::ImageSkia instance is owned by the window itself.
    window->SetProperty(aura::client::kWindowIconKey,
                        new gfx::ImageSkia(*icon));

    g_ksv_view->GetWidget()->Show();
    g_ksv_view->RequestFocusForActiveTab();
  }
  return g_ksv_view->GetWidget();
}

KeyboardShortcutView::KeyboardShortcutView() {
  DCHECK_EQ(g_ksv_view, nullptr);
  g_ksv_view = this;

  // Default background is transparent.
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
  InitViews();
}

void KeyboardShortcutView::InitViews() {
  // Init search box view.
  search_box_view_ = std::make_unique<KSVSearchBoxView>(this);
  search_box_view_->Init();
  AddChildView(search_box_view_.get());

  // Init start searching illustration view.
  search_start_view_ = std::make_unique<views::View>();
  SetupSearchIllustrationView(search_start_view_.get(), kKsvSearchStartIcon,
                              IDS_KSV_SEARCH_START);

  // Init no search result illustration view.
  search_no_result_view_ = std::make_unique<views::View>();
  SetupSearchIllustrationView(search_no_result_view_.get(),
                              kKsvSearchNoResultIcon, IDS_KSV_SEARCH_NO_RESULT);

  // Init search results container view.
  search_results_container_ = new views::View();
  search_results_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  search_results_container_->AddChildView(search_start_view_.get());
  search_results_container_->SetVisible(false);
  AddChildView(search_results_container_);

  // Init views of KeyboardShortcutItemView.
  for (const auto& item : GetKeyboardShortcutItemList()) {
    for (auto category : item.categories) {
      shortcut_views_.emplace_back(
          new KeyboardShortcutItemView(item, category));
    }
  }
  std::sort(shortcut_views_.begin(), shortcut_views_.end(),
            [](KeyboardShortcutItemView* lhs, KeyboardShortcutItemView* rhs) {
              if (lhs->category() != rhs->category())
                return lhs->category() < rhs->category();
              return lhs->description_label_view()->text() <
                     rhs->description_label_view()->text();
            });

  // Init views of TabbedPane and KeyboardShortcutItemListView.
  tabbed_pane_ =
      new views::TabbedPane(views::TabbedPane::Orientation::kVertical,
                            views::TabbedPane::TabStripStyle::kHighlight);
  ShortcutCategory current_category = ShortcutCategory::kUnknown;
  KeyboardShortcutItemListView* item_list_view;
  for (auto* item_view : shortcut_views_) {
    const ShortcutCategory category = item_view->category();
    DCHECK_NE(ShortcutCategory::kUnknown, category);
    if (current_category != category) {
      current_category = category;
      item_list_view = new KeyboardShortcutItemListView();
      views::ScrollView* const scroller = CreateScrollView();
      scroller->SetContents(item_list_view);
      tabbed_pane_->AddTab(GetStringForCategory(current_category), scroller);
    }
    if (item_list_view->has_children())
      item_list_view->AddHorizontalSeparator();
    item_list_view->AddChildView(item_view);
  }
  AddChildView(tabbed_pane_);
}

void KeyboardShortcutView::RequestFocusForActiveTab() {
  // Get the |tab_strip_| of the |tabbed_pane_| in order to set focus on
  // the selected tab.
  tabbed_pane_->child_at(0)
      ->child_at(tabbed_pane_->GetSelectedTabIndex())
      ->RequestFocus();
}

bool KeyboardShortcutView::CanMaximize() const {
  return false;
}

bool KeyboardShortcutView::CanMinimize() const {
  return true;
}

bool KeyboardShortcutView::CanResize() const {
  return false;
}

views::ClientView* KeyboardShortcutView::CreateClientView(
    views::Widget* widget) {
  return new views::ClientView(widget, this);
}

void KeyboardShortcutView::Layout() {
  gfx::Rect content_bounds(GetContentsBounds());
  if (content_bounds.IsEmpty())
    return;

  constexpr int kSearchBoxTopPadding = 8;
  constexpr int kSearchBoxBottomPadding = 16;
  constexpr int kSearchBoxHorizontalPadding = 32;
  const int left = content_bounds.x();
  const int top = content_bounds.y();
  gfx::Rect search_box_bounds(search_box_view_->GetPreferredSize());
  search_box_bounds.set_width(
      std::min(search_box_bounds.width(),
               content_bounds.width() - 2 * kSearchBoxHorizontalPadding));
  search_box_bounds.set_x(
      left + (content_bounds.width() - search_box_bounds.width()) / 2);
  search_box_bounds.set_y(top + kSearchBoxTopPadding);
  search_box_view_->SetBoundsRect(search_box_bounds);

  views::View* content_view =
      tabbed_pane_->visible() ? tabbed_pane_ : search_results_container_;

  const int search_box_used_height = search_box_bounds.height() +
                                     kSearchBoxTopPadding +
                                     kSearchBoxBottomPadding;
  content_view->SetBounds(left, top + search_box_used_height,
                          content_bounds.width(),
                          content_bounds.height() - search_box_used_height);
}

void KeyboardShortcutView::BackButtonPressed() {
  search_box_view_->SetSearchBoxActive(false);
  search_box_view_->ClearSearch();
}

void KeyboardShortcutView::QueryChanged(search_box::SearchBoxViewBase* sender) {
  search_results_container_->RemoveAllChildViews(true);
  const bool query_empty = sender->IsSearchBoxTrimmedQueryEmpty();
  auto* search_container_content_view = search_start_view_.get();
  if (!query_empty) {
    search_container_content_view = search_no_result_view_.get();

    auto found_items_list_view =
        std::make_unique<KeyboardShortcutItemListView>();
    const base::string16& new_contents = sender->search_box()->text();
    base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents finder(
        new_contents);
    ShortcutCategory current_category = ShortcutCategory::kUnknown;
    bool has_category_item = false;
    for (auto* item_view : shortcut_views_) {
      base::string16 description_text =
          item_view->description_label_view()->text();
      base::string16 shortcut_text = item_view->shortcut_label_view()->text();
      size_t match_index = -1;
      size_t match_length = 0;
      // Only highlight |description_label_view_| in KeyboardShortcutItemView.
      // |shortcut_label_view_| has customized style ranges for bubble views
      // so it may have overlappings with the searched ranges. The highlighted
      // behaviors are not defined so we don't highlight
      // |shortcut_label_view_|.
      if (finder.Search(description_text, &match_index, &match_length) ||
          finder.Search(shortcut_text, nullptr, nullptr)) {
        const ShortcutCategory category = item_view->category();
        if (current_category != category) {
          current_category = category;
          has_category_item = false;
          found_items_list_view->AddCategoryLabel(
              GetStringForCategory(category));
        }
        if (has_category_item)
          found_items_list_view->AddHorizontalSeparator();
        else
          has_category_item = true;
        auto* matched_item_view =
            new KeyboardShortcutItemView(*item_view->shortcut_item(), category);
        // Highlight matched query in |description_label_view_|.
        if (match_length > 0) {
          views::StyledLabel::RangeStyleInfo style;
          views::StyledLabel* description_label_view =
              matched_item_view->description_label_view();
          style.custom_font =
              description_label_view->GetDefaultFontList().Derive(
                  0, gfx::Font::FontStyle::NORMAL, gfx::Font::Weight::BOLD);
          description_label_view->AddStyleRange(
              gfx::Range(match_index, match_index + match_length), style);
        }

        found_items_list_view->AddChildView(matched_item_view);
      }
    }

    if (found_items_list_view->has_children()) {
      // To offset the padding between the bottom of the |search_box_view_| and
      // the top of the |search_results_container_|.
      constexpr int kTopPadding = -16;
      constexpr int kHorizontalPadding = 128;
      found_items_list_view->SetBorder(views::CreateEmptyBorder(
          gfx::Insets(kTopPadding, kHorizontalPadding, 0, kHorizontalPadding)));
      views::ScrollView* const scroller = CreateScrollView();
      scroller->SetContents(found_items_list_view.release());
      search_container_content_view = scroller;
    }
  }
  search_results_container_->AddChildView(search_container_content_view);
  Layout();
  SchedulePaint();
}

void KeyboardShortcutView::ActiveChanged(
    search_box::SearchBoxViewBase* sender) {
  const bool is_active = sender->is_search_box_active();
  sender->ShowBackOrGoogleIcon(is_active);
  search_results_container_->SetVisible(is_active);
  tabbed_pane_->SetVisible(!is_active);
  if (!is_active) {
    search_results_container_->RemoveAllChildViews(true);
    RequestFocusForActiveTab();
  }
  Layout();
  SchedulePaint();
}

KeyboardShortcutView* KeyboardShortcutView::GetInstanceForTesting() {
  return g_ksv_view;
}

int KeyboardShortcutView::GetTabCountForTesting() const {
  return tabbed_pane_->GetTabCount();
}

}  // namespace keyboard_shortcut_viewer
