// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_main_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_container_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/window.h"
#include "ui/chromeos/search_box/search_box_view_base.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// AppListMainView:

AppListMainView::AppListMainView(AppListViewDelegate* delegate,
                                 AppListView* app_list_view)
    : delegate_(delegate),
      model_(delegate->GetModel()),
      search_model_(delegate->GetSearchModel()),
      app_list_view_(app_list_view) {
  // We need a layer to apply transform to in small display so that the apps
  // grid fits in the display.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  model_->AddObserver(this);
}

AppListMainView::~AppListMainView() {
  model_->RemoveObserver(this);
}

void AppListMainView::Init(int initial_apps_page,
                           SearchBoxView* search_box_view) {
  search_box_view_ = search_box_view;
  AddContentsViews();

  // Switch the apps grid view to the specified page.
  PaginationModel* pagination_model = GetAppsPaginationModel();
  if (pagination_model->is_valid_page(initial_apps_page))
    pagination_model->SelectPage(initial_apps_page, false);
}

void AppListMainView::AddContentsViews() {
  DCHECK(search_box_view_);
  auto contents_view = std::make_unique<ContentsView>(app_list_view_);
  contents_view->Init(model_);
  contents_view->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  contents_view->layer()->SetMasksToBounds(true);
  contents_view_ = AddChildView(std::move(contents_view));

  search_box_view_->set_contents_view(contents_view_);
}

void AppListMainView::ShowAppListWhenReady() {
  // After switching to tablet mode, other app windows may be active. Show the
  // app list without activating it to avoid breaking other windows' state.
  const aura::Window* active_window =
      wm::GetActivationClient(
          app_list_view_->GetWidget()->GetNativeView()->GetRootWindow())
          ->GetActiveWindow();
  if (app_list_view_->is_tablet_mode() && active_window)
    GetWidget()->ShowInactive();
  else
    GetWidget()->Show();
}

void AppListMainView::ModelChanged() {
  model_->RemoveObserver(this);
  model_ = delegate_->GetModel();
  model_->AddObserver(this);
  search_model_ = delegate_->GetSearchModel();
  search_box_view_->ModelChanged();
  delete contents_view_;
  contents_view_ = nullptr;
  AddContentsViews();
  Layout();
}

void AppListMainView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  contents_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

PaginationModel* AppListMainView::GetAppsPaginationModel() {
  return contents_view_->apps_container_view()
      ->apps_grid_view()
      ->pagination_model();
}

void AppListMainView::NotifySearchBoxVisibilityChanged() {
  // Repaint the AppListView's background which will repaint the background for
  // the search box. This is needed because this view paints to a layer and
  // won't propagate paints upward.
  if (parent())
    parent()->SchedulePaint();
}

const char* AppListMainView::GetClassName() const {
  return "AppListMainView";
}

void AppListMainView::Layout() {
  gfx::Rect rect = GetContentsBounds();
  if (!rect.IsEmpty())
    contents_view_->SetBoundsRect(rect);
}

void AppListMainView::ActivateApp(AppListItem* item, int event_flags) {
  // TODO(jennyz): Activate the folder via AppListModel notification.
  if (item->GetItemType() == AppListFolderItem::kItemType) {
    contents_view_->ShowFolderContent(static_cast<AppListFolderItem*>(item));
    UMA_HISTOGRAM_ENUMERATION(kAppListFolderOpenedHistogram,
                              kFullscreenAppListFolders, kMaxFolderOpened);
  } else {
    base::RecordAction(base::UserMetricsAction("AppList_ClickOnApp"));

    // Avoid using |item->id()| as the parameter. In some rare situations,
    // activating the item may destruct it. Using the reference to an object
    // which may be destroyed during the procedure as the function parameter
    // may bring the crash like https://crbug.com/990282.
    const std::string id = item->id();
    delegate_->ActivateItem(id, event_flags,
                            AppListLaunchedFrom::kLaunchedFromGrid);
  }
}

void AppListMainView::CancelDragInActiveFolder() {
  contents_view_->apps_container_view()
      ->app_list_folder_view()
      ->items_grid_view()
      ->EndDrag(true);
}

void AppListMainView::OnResultInstalled(SearchResult* result) {
  // Clears the search to show the apps grid. The last installed app
  // should be highlighted and made visible already.
  search_box_view_->ClearSearch();
}

// AppListModelObserver overrides:
void AppListMainView::OnAppListStateChanged(AppListState new_state,
                                            AppListState old_state) {
  if (new_state == AppListState::kStateEmbeddedAssistant) {
    search_box_view_->SetVisible(false);
  } else {
    search_box_view_->SetVisible(true);
  }
}

void AppListMainView::QueryChanged(search_box::SearchBoxViewBase* sender) {
  base::string16 raw_query = search_model_->search_box()->text();
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
  bool should_show_search =
      app_list_features::IsZeroStateSuggestionsEnabled()
          ? search_box_view_->is_search_box_active() || !query.empty()
          : !query.empty();
  contents_view_->ShowSearchResults(should_show_search);

  delegate_->StartSearch(raw_query);
}

void AppListMainView::ActiveChanged(search_box::SearchBoxViewBase* sender) {
  if (!app_list_features::IsZeroStateSuggestionsEnabled())
    return;
  // Do not update views on closing.
  if (app_list_view_->app_list_state() == AppListViewState::kClosed)
    return;

  if (search_box_view_->is_search_box_active()) {
    // Show zero state suggestions when search box is activated with an empty
    // query.
    base::string16 raw_query = search_model_->search_box()->text();
    base::string16 query;
    base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
    if (query.empty())
      search_box_view_->ShowZeroStateSuggestions();
  } else {
    // Close the search results page if the search box is inactive.
    contents_view_->ShowSearchResults(false);
  }
}

void AppListMainView::SearchBoxFocusChanged(
    search_box::SearchBoxViewBase* sender) {
  // A fake focus (highlight) is always set on the first search result. When the
  // user moves focus from the search box textfield (e.g. to close button or
  // last search result), the fake focus should be removed.
  if (sender->search_box()->HasFocus())
    return;

  SearchResultBaseView* first_result_view =
      contents_view_->search_results_page_view()->first_result_view();
  if (!first_result_view || !first_result_view->selected())
    return;
  first_result_view->SetSelected(false, base::nullopt);
}

void AppListMainView::AssistantButtonPressed() {
  delegate_->StartAssistant();
}

void AppListMainView::BackButtonPressed() {
  if (!contents_view_->Back())
    app_list_view_->Dismiss();
}

}  // namespace ash
