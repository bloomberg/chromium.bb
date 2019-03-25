// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_

#include <memory>
#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "base/macros.h"
#include "ui/views/context_menu_controller.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace app_list {

class AppListViewDelegate;
class SearchResult;
class PaginationModel;

// A tile view that displays a search result. It hosts view for search result
// that has SearchResult::DisplayType DISPLAY_TILE or DISPLAY_RECOMMENDATION.
class APP_LIST_EXPORT SearchResultTileItemView
    : public SearchResultBaseView,
      public views::ContextMenuController,
      public AppListMenuModelAdapter::Delegate {
 public:
  SearchResultTileItemView(AppListViewDelegate* view_delegate,
                           PaginationModel* pagination_model,
                           bool show_in_apps_page);
  ~SearchResultTileItemView() override;

  void OnResultChanged() override;
  void SetIndexInItemListView(size_t index);

  // Informs the SearchResultTileItemView of its parent's background color. The
  // controls within the SearchResultTileItemView will adapt to suit the given
  // color.
  void SetParentBackgroundColor(SkColor color);

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::Button:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  void StateChanged(ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Overridden from SearchResultObserver:
  void OnMetadataChanged() override;

  // views::ContextMenuController overrides:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // AppListMenuModelAdapter::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // Launch the result and log to various histograms.
  void ActivateResult(int event_flags);

  // Bound by ShowContextMenuForViewImpl().
  void OnGetContextMenuModel(views::View* source,
                             const gfx::Point& point,
                             ui::MenuSourceType source_type,
                             std::vector<ash::mojom::MenuItemPtr> menu);

  // The callback used when a menu closes.
  void OnMenuClosed();

  void SetIcon(const gfx::ImageSkia& icon);
  void SetBadgeIcon(const gfx::ImageSkia& badge_icon);
  void SetTitle(const base::string16& title);
  void SetRating(float rating);
  void SetPrice(const base::string16& price);

  AppListMenuModelAdapter::AppListViewAppType GetAppType() const;

  // Whether the tile view is a suggested app.
  bool IsSuggestedAppTile() const;
  // Whether the tile view is a suggested app and shown in apps page ui.
  bool IsSuggestedAppTileShownInAppPage() const;

  // Records an app being launched.
  void LogAppLaunchForSuggestedApp() const;

  void UpdateBackgroundColor();

  // Overridden from views::View:
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool GetTooltipText(const gfx::Point& p,
                      base::string16* tooltip) const override;

  AppListViewDelegate* const view_delegate_;           // Owned by AppListView.
  PaginationModel* const pagination_model_;            // Owned by AppsGridView.

  views::ImageView* icon_ = nullptr;         // Owned by views hierarchy.
  views::ImageView* badge_ = nullptr;        // Owned by views hierarchy.
  views::Label* title_ = nullptr;            // Owned by views hierarchy.
  views::Label* rating_ = nullptr;           // Owned by views hierarchy.
  views::Label* price_ = nullptr;            // Owned by views hierarchy.
  views::ImageView* rating_star_ = nullptr;  // Owned by views hierarchy.

  SkColor parent_background_color_ = SK_ColorTRANSPARENT;

  const bool is_play_store_app_search_enabled_;
  const bool is_app_reinstall_recommendation_enabled_;
  const bool show_in_apps_page_;  // True if shown in app list's apps page.

  std::unique_ptr<AppListMenuModelAdapter> context_menu_;

  // The index of this item in the search_result_tile_item_list_view, only used
  // for logging.
  int index_in_item_list_view_ = -1;
  base::WeakPtrFactory<SearchResultTileItemView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemView);
};

}  // namespace app_list

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_VIEW_H_
