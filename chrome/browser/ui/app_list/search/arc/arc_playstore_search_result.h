// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "components/arc/common/app.mojom.h"
#include "ui/app_list/search_result.h"

class AppListControllerDelegate;
class ArcPlayStoreAppContextMenu;
class Profile;

namespace app_list {

class ArcPlayStoreSearchResult : public SearchResult,
                                 public AppContextMenuDelegate {
 public:
  ArcPlayStoreSearchResult(arc::mojom::AppDiscoveryResultPtr data,
                           Profile* profile,
                           AppListControllerDelegate* list_controller_);
  ~ArcPlayStoreSearchResult() override;

  // app_list::SearchResult overrides:
  std::unique_ptr<SearchResult> Duplicate() const override;

  // app_list::AppContextMenuDelegate overrides:
  ui::MenuModel* GetContextMenuModel() override;
  void Open(int event_flags) override;
  void ExecuteLaunchCommand(int event_flags) override;

  // Disables async safe decoding requests when unit tests are executed.
  // Icons are decoded at a separate process created by ImageDecoder. In unit
  // tests these tasks may not finish before the test exits, which causes a
  // failure in the base::MessageLoop::current()->IsIdleForTesting() check
  // in test_browser_thread_bundle.cc.
  static void DisableSafeDecodingForTesting();

 private:
  class IconDecodeRequest;

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

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_RESULT_H_
