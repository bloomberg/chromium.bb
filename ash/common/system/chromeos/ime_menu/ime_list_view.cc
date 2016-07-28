// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"

#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A |HoverHighlightView| that uses bold or normal font depending on whether it
// is selected.  This view exposes itself as a checkbox to the accessibility
// framework.
class SelectableHoverHighlightView : public HoverHighlightView {
 public:
  SelectableHoverHighlightView(ViewClickListener* listener,
                               const base::string16& label,
                               bool selected)
      : HoverHighlightView(listener), selected_(selected) {
    AddLabel(label, gfx::ALIGN_LEFT, selected);
  }

  ~SelectableHoverHighlightView() override {}

 protected:
  // views::View:
  void GetAccessibleState(ui::AXViewState* state) override {
    HoverHighlightView::GetAccessibleState(state);
    state->role = ui::AX_ROLE_CHECK_BOX;
    if (selected_)
      state->AddStateFlag(ui::AX_STATE_CHECKED);
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SelectableHoverHighlightView);
};

}  // namespace

ImeListView::ImeListView(SystemTrayItem* owner,
                         bool show_keyboard_toggle,
                         SingleImeBehavior single_ime_behavior)
    : TrayDetailsView(owner) {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  IMEInfoList list;
  delegate->GetAvailableIMEList(&list);
  IMEPropertyInfoList property_list;
  delegate->GetCurrentIMEProperties(&property_list);
  Update(list, property_list, show_keyboard_toggle, single_ime_behavior);
}

ImeListView::~ImeListView() {}

void ImeListView::Update(const IMEInfoList& list,
                         const IMEPropertyInfoList& property_list,
                         bool show_keyboard_toggle,
                         SingleImeBehavior single_ime_behavior) {
  Reset();
  ime_map_.clear();
  property_map_.clear();
  CreateScrollableList();

  // Appends IME list and IME properties.
  if (single_ime_behavior == ImeListView::SHOW_SINGLE_IME || list.size() > 1) {
    AppendIMEList(list);
    if (!property_list.empty())
      AppendIMEProperties(property_list);
  }

  if (show_keyboard_toggle) {
    if (list.size() > 1 || !property_list.empty())
      AddScrollSeparator();
    AppendKeyboardStatus();
  }

  Layout();
  SchedulePaint();
}

void ImeListView::AppendIMEList(const IMEInfoList& list) {
  DCHECK(ime_map_.empty());
  for (size_t i = 0; i < list.size(); i++) {
    HoverHighlightView* container =
        new SelectableHoverHighlightView(this, list[i].name, list[i].selected);
    scroll_content()->AddChildView(container);
    ime_map_[container] = list[i].id;
  }
}

void ImeListView::AppendIMEProperties(
    const IMEPropertyInfoList& property_list) {
  DCHECK(property_map_.empty());
  for (size_t i = 0; i < property_list.size(); i++) {
    HoverHighlightView* container = new SelectableHoverHighlightView(
        this, property_list[i].name, property_list[i].selected);
    if (i == 0)
      container->SetBorder(
          views::Border::CreateSolidSidedBorder(1, 0, 0, 0, kBorderLightColor));
    scroll_content()->AddChildView(container);
    property_map_[container] = property_list[i].key;
  }
}

void ImeListView::AppendKeyboardStatus() {
  HoverHighlightView* container = new HoverHighlightView(this);
  int id = keyboard::IsKeyboardEnabled() ? IDS_ASH_STATUS_TRAY_DISABLE_KEYBOARD
                                         : IDS_ASH_STATUS_TRAY_ENABLE_KEYBOARD;
  container->AddLabel(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(id),
      gfx::ALIGN_LEFT, false /* highlight */);
  scroll_content()->AddChildView(container);
  keyboard_status_ = container;
}

void ImeListView::OnViewClicked(views::View* sender) {
  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  if (sender == keyboard_status_) {
    WmShell::Get()->ToggleIgnoreExternalKeyboard();
  } else {
    std::map<views::View*, std::string>::const_iterator ime_find;
    ime_find = ime_map_.find(sender);
    if (ime_find != ime_map_.end()) {
      WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SWITCH_MODE);
      std::string ime_id = ime_find->second;
      delegate->SwitchIME(ime_id);
      GetWidget()->Close();
    } else {
      std::map<views::View*, std::string>::const_iterator prop_find;
      prop_find = property_map_.find(sender);
      if (prop_find != property_map_.end()) {
        const std::string key = prop_find->second;
        delegate->ActivateIMEProperty(key);
        GetWidget()->Close();
      }
    }
  }
}

}  // namespace ash
