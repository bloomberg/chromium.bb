// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/search/search_result.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/arc/common/app.mojom.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

class IconDecodeRequest;

class ArcAppDataSearchResult : public SearchResult {
 public:
  ArcAppDataSearchResult(arc::mojom::AppDataResultPtr data,
                         Profile* profile,
                         AppListControllerDelegate* list_controller);
  ~ArcAppDataSearchResult() override;

  // app_list::SearchResult:
  std::unique_ptr<SearchResult> Duplicate() const override;
  ui::MenuModel* GetContextMenuModel() override;
  void Open(int event_flags) override;

 private:
  const std::string& launch_intent_uri() const {
    return data_->launch_intent_uri;
  }
  const std::string& label() const { return data_->label; }
  const base::Optional<std::vector<uint8_t>>& icon_png_data() const {
    return data_->icon_png_data;
  }

  // Apply avatar style to |icon| and use it for SearchResult.
  void SetIconToAvatarIcon(const gfx::ImageSkia& icon);

  arc::mojom::AppDataResultPtr data_;
  std::unique_ptr<IconDecodeRequest> icon_decode_request_;

  // |profile_| is owned by ProfileInfo.
  Profile* const profile_;
  // |list_controller_| is owned by AppListServiceAsh and lives until the
  // service finishes.
  AppListControllerDelegate* const list_controller_;

  base::WeakPtrFactory<ArcAppDataSearchResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDataSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_RESULT_H_
