// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_group_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

TabGroup::TabGroup(TabGroupController* controller,
                   const tab_groups::TabGroupId& id,
                   const tab_groups::TabGroupVisualData& visual_data)
    : controller_(controller), id_(id) {
  visual_data_ = std::make_unique<tab_groups::TabGroupVisualData>(visual_data);
}

TabGroup::~TabGroup() = default;

void TabGroup::SetVisualData(const tab_groups::TabGroupVisualData& visual_data,
                             bool is_customized) {
  visual_data_ = std::make_unique<tab_groups::TabGroupVisualData>(visual_data);
  is_customized_ = is_customized;
  controller_->ChangeTabGroupVisuals(id_);
}

base::string16 TabGroup::GetContentString() const {
  std::vector<int> tabs_in_group = ListTabs();
  TabUIHelper* const tab_ui_helper = TabUIHelper::FromWebContents(
      controller_->GetWebContentsAt(tabs_in_group.front()));
  constexpr size_t kContextMenuTabTitleMaxLength = 30;
  base::string16 format_string = l10n_util::GetPluralStringFUTF16(
      IDS_TAB_CXMENU_PLACEHOLDER_GROUP_TITLE, tabs_in_group.size() - 1);
  base::string16 short_title;
  gfx::ElideString(tab_ui_helper->GetTitle(), kContextMenuTabTitleMaxLength,
                   &short_title);
  return base::ReplaceStringPlaceholders(format_string, {short_title}, nullptr);
}

void TabGroup::AddTab() {
  if (tab_count_ == 0) {
    controller_->CreateTabGroup(id_);
    controller_->ChangeTabGroupVisuals(id_);
  }
  controller_->ChangeTabGroupContents(id_);
  ++tab_count_;
}

void TabGroup::RemoveTab() {
  DCHECK_GT(tab_count_, 0);
  --tab_count_;
  if (tab_count_ == 0)
    controller_->CloseTabGroup(id_);
  else
    controller_->ChangeTabGroupContents(id_);
}

bool TabGroup::IsEmpty() const {
  return tab_count_ == 0;
}

bool TabGroup::IsCustomized() const {
  return is_customized_;
}

std::vector<int> TabGroup::ListTabs() const {
  std::vector<int> result;
  for (int i = 0; i < controller_->GetTabCount(); ++i) {
    if (controller_->GetTabGroupForTab(i) == id_)
      result.push_back(i);
  }
  return result;
}
