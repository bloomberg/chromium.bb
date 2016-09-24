// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Creates a separator that will be used between the IME list items.
views::Separator* CreateListItemSeparator() {
  views::Separator* separator =
      new views::Separator(views::Separator::HORIZONTAL);
  separator->SetColor(kBorderLightColor);
  separator->SetPreferredSize(kSeparatorWidth);
  separator->SetBorder(views::Border::CreateEmptyBorder(
      kMenuSeparatorVerticalPadding, 0, kMenuSeparatorVerticalPadding, 0));
  return separator;
}

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

// The view that contains IME short name and the IME label.
class ImeInfoView : public views::View {
 public:
  ImeInfoView(const base::string16& id, const base::string16& text) {
    views::BoxLayout* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    SetLayoutManager(box_layout);

    // TODO(azurewei): Use TrayPopupItemStyle for |id_button|.
    views::LabelButton* id_button = new views::LabelButton(nullptr, id);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    id_button->SetFontList(rb.GetFontList(ui::ResourceBundle::MediumBoldFont));
    id_button->SetTextColor(views::Button::STATE_NORMAL, kMenuIconColor);
    id_button->SetMaxSize(gfx::Size(kMenuButtonSize, kMenuButtonSize));
    id_button->SetMinSize(gfx::Size(kMenuButtonSize, kMenuButtonSize));
    const int button_padding = (kMenuButtonSize - kMenuIconSize) / 2;
    id_button->SetBorder(views::Border::CreateEmptyBorder(
        button_padding, button_padding, button_padding, button_padding));
    AddChildView(id_button);

    views::Label* text_label = new views::Label(text);
    TrayPopupItemStyle style(
        GetNativeTheme(), TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(text_label);
    text_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    AddChildView(text_label);
    box_layout->SetFlexForView(text_label, 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeInfoView);
};

// The IME list item view used in the material design. It contains IME info
// (name and label) and a check button if the item is selected. It's also used
// for IME property item, which has no name but label and a gray checked icon.
class ImeListItemView : public ActionableView {
 public:
  ImeListItemView(SystemTrayItem* owner,
                  ImeListView* list_view,
                  const base::string16& id,
                  const base::string16& label,
                  bool selected,
                  const SkColor button_color)
      : ActionableView(owner), ime_list_view_(list_view) {
    views::BoxLayout* box_layout =
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
    SetLayoutManager(box_layout);

    ImeInfoView* info_view = new ImeInfoView(id, label);
    AddChildView(info_view);
    box_layout->SetFlexForView(info_view, 1);

    if (selected) {
      views::ImageButton* check_button = new views::ImageButton(nullptr);
      gfx::ImageSkia icon_image = gfx::CreateVectorIcon(
          gfx::VectorIconId::CHECK_CIRCLE, kMenuIconSize, button_color);
      check_button->SetImage(views::CustomButton::STATE_NORMAL, &icon_image);
      const int button_padding = (kMenuButtonSize - icon_image.width()) / 2;
      check_button->SetBorder(views::Border::CreateEmptyBorder(
          button_padding, button_padding, button_padding, button_padding));
      AddChildView(check_button);
    }
  }

  ~ImeListItemView() override {}

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    ime_list_view_->HandleViewClicked(this);
    return true;
  }

 private:
  ImeListView* ime_list_view_;
  DISALLOW_COPY_AND_ASSIGN(ImeListItemView);
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
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      AppendImeListAndProperties(list, property_list);
    } else {
      AppendIMEList(list);
      if (!property_list.empty())
        AppendIMEProperties(property_list);
    }
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

void ImeListView::AppendImeListAndProperties(
    const IMEInfoList& list,
    const IMEPropertyInfoList& property_list) {
  DCHECK(ime_map_.empty());
  for (size_t i = 0; i < list.size(); i++) {
    views::View* ime_view =
        new ImeListItemView(owner(), this, list[i].short_name, list[i].name,
                            list[i].selected, gfx::kGoogleGreen700);
    scroll_content()->AddChildView(ime_view);
    ime_map_[ime_view] = list[i].id;

    // In material design, the property items will be added after the current
    // selected IME item.
    if (list[i].selected && !property_list.empty()) {
      // Adds a separator on the top of property items.
      scroll_content()->AddChildView(CreateListItemSeparator());

      // Adds the property items.
      for (size_t i = 0; i < property_list.size(); i++) {
        ImeListItemView* property_view = new ImeListItemView(
            owner(), this, base::string16(), property_list[i].name,
            property_list[i].selected, kMenuIconColor);
        scroll_content()->AddChildView(property_view);
        property_map_[property_view] = property_list[i].key;
      }

      // Adds a separator on the bottom of property items.
      scroll_content()->AddChildView(CreateListItemSeparator());
    }
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

void ImeListView::HandleViewClicked(views::View* view) {
  if (view == keyboard_status_) {
    WmShell::Get()->ToggleIgnoreExternalKeyboard();
    return;
  }

  SystemTrayDelegate* delegate = WmShell::Get()->system_tray_delegate();
  std::map<views::View*, std::string>::const_iterator ime = ime_map_.find(view);
  if (ime != ime_map_.end()) {
    WmShell::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SWITCH_MODE);
    std::string ime_id = ime->second;
    delegate->SwitchIME(ime_id);
  } else {
    std::map<views::View*, std::string>::const_iterator property =
        property_map_.find(view);
    if (property == property_map_.end())
      return;
    const std::string key = property->second;
    delegate->ActivateIMEProperty(key);
  }

  GetWidget()->Close();
}

}  // namespace ash
