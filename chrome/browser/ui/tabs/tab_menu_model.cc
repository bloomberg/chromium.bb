// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_menu_model.h"

#include "base/command_line.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "ui/base/l10n/l10n_util.h"

TabMenuModel::TabMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                           TabStripModel* tab_strip,
                           int index)
    : ui::SimpleMenuModel(delegate) {
  Build(tab_strip, index);
}

void TabMenuModel::Build(TabStripModel* tab_strip, int index) {
  std::vector<int> affected_indices =
      tab_strip->IsTabSelected(index)
          ? tab_strip->selection_model().selected_indices()
          : std::vector<int>{index};
  int num_affected_tabs = affected_indices.size();
  AddItemWithStringId(TabStripModel::CommandNewTab, IDS_TAB_CXMENU_NEWTAB);
  if (base::FeatureList::IsEnabled(features::kTabGroups)) {
    AddItemWithStringId(TabStripModel::CommandAddToNewGroup,
                        IDS_TAB_CXMENU_ADD_TAB_TO_NEW_GROUP);
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(TabStripModel::CommandReload, IDS_TAB_CXMENU_RELOAD);
  AddItemWithStringId(TabStripModel::CommandDuplicate,
                      IDS_TAB_CXMENU_DUPLICATE);
  bool will_pin = tab_strip->WillContextMenuPin(index);
  AddItem(TabStripModel::CommandTogglePinned,
          will_pin ? l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_PIN_TAB,
                                                      num_affected_tabs)
                   : l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_UNPIN_TAB,
                                                      num_affected_tabs));
  if (base::FeatureList::IsEnabled(features::kSoundContentSetting)) {
    const bool will_mute =
        !chrome::AreAllSitesMuted(*tab_strip, affected_indices);
    AddItem(TabStripModel::CommandToggleSiteMuted,
            will_mute
                ? l10n_util::GetPluralStringFUTF16(
                      IDS_TAB_CXMENU_SOUND_MUTE_SITE, num_affected_tabs)
                : l10n_util::GetPluralStringFUTF16(
                      IDS_TAB_CXMENU_SOUND_UNMUTE_SITE, num_affected_tabs));
  } else {
    const bool will_mute =
        !chrome::AreAllTabsMuted(*tab_strip, affected_indices);
    AddItem(TabStripModel::CommandToggleTabAudioMuted,
            will_mute
                ? l10n_util::GetPluralStringFUTF16(
                      IDS_TAB_CXMENU_AUDIO_MUTE_TAB, num_affected_tabs)
                : l10n_util::GetPluralStringFUTF16(
                      IDS_TAB_CXMENU_AUDIO_UNMUTE_TAB, num_affected_tabs));
  }
  if (base::FeatureList::IsEnabled(switches::kSyncSendTabToSelf)) {
    AddItemWithStringId(TabStripModel::CommandSendToMyDevices,
                        IDS_TAB_CXMENU_SEND_TO_MY_DEVICES);
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItem(TabStripModel::CommandCloseTab,
          l10n_util::GetPluralStringFUTF16(IDS_TAB_CXMENU_CLOSETAB,
                                           num_affected_tabs));
  AddItemWithStringId(TabStripModel::CommandCloseOtherTabs,
                      IDS_TAB_CXMENU_CLOSEOTHERTABS);
  AddItemWithStringId(TabStripModel::CommandCloseTabsToRight,
                      IDS_TAB_CXMENU_CLOSETABSTORIGHT);
  AddSeparator(ui::NORMAL_SEPARATOR);

  const bool is_window = tab_strip->delegate()->GetRestoreTabType() ==
      TabStripModelDelegate::RESTORE_WINDOW;
  AddItemWithStringId(TabStripModel::CommandRestoreTab,
                      is_window ? IDS_RESTORE_WINDOW : IDS_RESTORE_TAB);
  AddItemWithStringId(TabStripModel::CommandBookmarkAllTabs,
                      IDS_TAB_CXMENU_BOOKMARK_ALL_TABS);
}
