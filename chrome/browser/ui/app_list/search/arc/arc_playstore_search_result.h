// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "components/arc/common/app.mojom.h"

class AppListControllerDelegate;
class ArcPlayStoreAppContextMenu;
class Profile;

namespace app_list {

class IconDecodeRequest;

class ArcPlayStoreSearchResult : public SearchResult,
                                 public AppContextMenuDelegate {
 public:
  ArcPlayStoreSearchResult(arc::mojom::AppDiscoveryResultPtr data,
                           Profile* profile,
                           AppListControllerDelegate* list_controller_);
  ~ArcPlayStoreSearchResult() override;

  // app_list::SearchResult overrides:
  std::unique_ptr<SearchResult> Duplicate() const override;
  ui::MenuModel* GetContextMenuModel() override;
  void Open(int event_flags) override;

  // app_list::AppContextMenuDelegate overrides:
  void ExecuteLaunchCommand(int event_flags) override;

 private:
  const base::Optional<std::string>& install_intent_uri() const {
    return data_->install_intent_uri;
  }
  const base::Optional<std::string>& label() const { return data_->label; }
  bool is_instant_app() const { return data_->is_instant_app; }
  const base::Optional<std::string>& formatted_price() const {
    return data_->formatted_price;
  }
  float review_score() const { return data_->review_score; }
  const std::vector<uint8_t>& icon_png_data() const {
    return data_->icon_png_data;
  }

  arc::mojom::AppDiscoveryResultPtr data_;
  std::unique_ptr<IconDecodeRequest> icon_decode_request_;

  // |profile_| is owned by ProfileInfo.
  Profile* const profile_;
  // |list_controller_| is owned by AppListServiceAsh and lives
  // until the service finishes.
  AppListControllerDelegate* const list_controller_;
  std::unique_ptr<ArcPlayStoreAppContextMenu> context_menu_;

  base::WeakPtrFactory<ArcPlayStoreSearchResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
