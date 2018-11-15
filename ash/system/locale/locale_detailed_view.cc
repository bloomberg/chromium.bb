// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/locale/locale_detailed_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "base/logging.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {
namespace tray {

LocaleDetailedView::LocaleDetailedView(DetailedViewDelegate* delegate)
    : TrayDetailedView(delegate) {
  CreateItems();
}

LocaleDetailedView::~LocaleDetailedView() = default;

void LocaleDetailedView::CreateItems() {
  CreateTitleRow(IDS_ASH_STATUS_TRAY_LOCALE_TITLE);
  CreateScrollableList();

  const std::vector<mojom::LocaleInfoPtr>& locales =
      Shell::Get()->system_tray_model()->locale_list();
  int id = 0;
  for (auto& entry : locales) {
    const bool checked =
        entry->iso_code ==
        Shell::Get()->system_tray_model()->current_locale_iso_code();
    HoverHighlightView* item =
        AddScrollListCheckableItem(entry->display_name, checked);
    item->set_id(id);
    id_to_locale_[id] = entry->iso_code;
    ++id;
  }
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
  // TODO(wzang): Show locale ISO codes at the beginning of each row, most
  // likely by reusing the code in |ImeListItemView|.
}

void LocaleDetailedView::HandleViewClicked(views::View* view) {
  auto it = id_to_locale_.find(view->id());
  DCHECK(it != id_to_locale_.end());
  const std::string locale_iso_code = it->second;
  if (locale_iso_code !=
      Shell::Get()->system_tray_model()->current_locale_iso_code()) {
    Shell::Get()->system_tray_model()->client_ptr()->SetLocaleAndExit(
        locale_iso_code);
  }
}

}  // namespace tray
}  // namespace ash