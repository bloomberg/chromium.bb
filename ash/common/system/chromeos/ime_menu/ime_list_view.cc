// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/ime_menu/ime_list_view.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/hover_highlight_view.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/system_menu_button.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ash/common/system/tray/tray_popup_header_button.h"
#include "ash/common/system/tray/tray_popup_item_style.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const int kMinFontSizeDelta = -10;

const SkColor kKeyboardRowSeparatorColor = SkColorSetA(SK_ColorBLACK, 0x1F);

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
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    HoverHighlightView::GetAccessibleNodeData(node_data);
    node_data->role = ui::AX_ROLE_CHECK_BOX;
    if (selected_)
      node_data->AddStateFlag(ui::AX_STATE_CHECKED);
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(SelectableHoverHighlightView);
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
      : ActionableView(owner, TrayPopupInkDropStyle::FILL_BOUNDS),
        ime_list_view_(list_view),
        selected_(selected) {
    if (MaterialDesignController::IsSystemTrayMenuMaterial())
      SetInkDropMode(InkDropHostView::InkDropMode::ON);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    SetLayoutManager(new views::FillLayout);

    // The id button shows the IME short name.
    views::Label* id_label = new views::Label(id);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& base_font_list =
        rb.GetFontList(ui::ResourceBundle::MediumBoldFont);
    id_label->SetFontList(base_font_list);

    // TODO(bruthig): Fix this so that |label| uses the kBackgroundColor to
    // perform subpixel rendering instead of disabling subpixel rendering.
    //
    // Text rendered on a non-opaque background looks ugly and it is possible
    // for labels to given a a clear canvas at paint time when an ink drop is
    // visible. See http://crbug.com/661714.
    id_label->SetSubpixelRenderingEnabled(false);

    // For IMEs whose short name are more than 2 characters (INTL, EXTD, etc.),
    // |kMenuIconSize| is not enough. The label will trigger eliding as "I..."
    // or "...". So we shrink the font size until it fits within the bounds.
    int size_delta = -1;
    while (id_label->GetPreferredSize().width() > kMenuIconSize &&
           size_delta >= kMinFontSizeDelta) {
      id_label->SetFontList(base_font_list.DeriveWithSizeDelta(size_delta));
      --size_delta;
    }
    tri_view->AddView(TriView::Container::START, id_label);

    // The label shows the IME name.
    label_ = TrayPopupUtils::CreateDefaultLabel();
    label_->SetText(label);
    UpdateStyle();
    label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, label_);

    if (selected) {
      // The checked button indicates the IME is selected.
      views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView();
      checked_image->SetImage(gfx::CreateVectorIcon(
          gfx::VectorIconId::CHECK_CIRCLE, kMenuIconSize, button_color));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    SetAccessibleName(label_->text());
  }

  ~ImeListItemView() override {}

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    ime_list_view_->HandleViewClicked(this);
    return true;
  }

  void OnFocus() override {
    ActionableView::OnFocus();
    if (ime_list_view_ && ime_list_view_->scroll_content())
      ime_list_view_->scroll_content()->ScrollRectToVisible(bounds());
  }

  // views::View:
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    UpdateStyle();
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ActionableView::GetAccessibleNodeData(node_data);
    node_data->role = ui::AX_ROLE_CHECK_BOX;
    node_data->AddStateFlag(selected_ ? ui::AX_STATE_CHECKED
                                      : ui::AX_STATE_NONE);
  }

 private:
  // Updates the style of |label_| based on the current native theme.
  void UpdateStyle() {
    TrayPopupItemStyle style(
        GetNativeTheme(), TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(label_);
  }

  // The label shows the IME name.
  views::Label* label_;
  ImeListView* ime_list_view_;
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(ImeListItemView);
};

}  // namespace

// The view that contains a |KeyboardButtonView| and a toggle button.
class MaterialKeyboardStatusRowView : public views::View {
 public:
  MaterialKeyboardStatusRowView(views::ButtonListener* listener, bool enabled)
      : listener_(listener), label_(nullptr), toggle_(nullptr) {
    Init();
    toggle_->SetIsOn(enabled, false);
  }

  ~MaterialKeyboardStatusRowView() override {}

  const views::Button* toggle() const { return toggle_; }
  bool is_toggled() const { return toggle_->is_on(); }

 protected:
  // views::View:
  int GetHeightForWidth(int w) const override {
    return GetPreferredSize().height();
  }

  void OnNativeThemeChanged(const ui::NativeTheme* theme) override {
    UpdateStyle();
  }

 private:
  void Init() {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);

    // The on-screen keyboard image button.
    views::ImageView* keyboard_image = TrayPopupUtils::CreateMainImageView();
    keyboard_image->SetImage(gfx::CreateVectorIcon(
        kImeMenuOnScreenKeyboardIcon, kMenuIconSize, kMenuIconColor));
    tri_view->AddView(TriView::Container::START, keyboard_image);

    // The on-screen keyboard label.
    label_ = TrayPopupUtils::CreateDefaultLabel();
    label_->SetText(ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
    UpdateStyle();
    tri_view->AddView(TriView::Container::CENTER, label_);

    // The on-screen keyboard toggle button.
    toggle_ = TrayPopupUtils::CreateToggleButton(
        listener_, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    tri_view->AddView(TriView::Container::END, toggle_);
  }

  // Updates the style of |label_| based on the current native theme.
  void UpdateStyle() {
    TrayPopupItemStyle style(
        GetNativeTheme(), TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(label_);
  }

  // ButtonListener to notify when |toggle_| is clicked.
  views::ButtonListener* listener_;

  // Label to with text 'On-screen keyboard'.
  views::Label* label_;

  // ToggleButton to toggle keyboard on or off.
  views::ToggleButton* toggle_;

  DISALLOW_COPY_AND_ASSIGN(MaterialKeyboardStatusRowView);
};

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
  ResetImeListView();
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
    if (MaterialDesignController::IsSystemTrayMenuMaterial()) {
      PrependMaterialKeyboardStatus();
    } else {
      if (list.size() > 1 || !property_list.empty())
        AddScrollSeparator();
      AppendKeyboardStatus();
    }
  }

  Layout();
  SchedulePaint();
}

void ImeListView::ResetImeListView() {
  // Children are removed from the view hierarchy and deleted in Reset().
  Reset();
  material_keyboard_status_view_ = nullptr;
  keyboard_status_ = nullptr;
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
          views::CreateSolidSidedBorder(1, 0, 0, 0, kBorderLightColor));
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
      scroll_content()->AddChildView(
          TrayPopupUtils::CreateListItemSeparator(true));

      // Adds the property items.
      for (size_t i = 0; i < property_list.size(); i++) {
        ImeListItemView* property_view = new ImeListItemView(
            owner(), this, base::string16(), property_list[i].name,
            property_list[i].selected, kMenuIconColor);
        scroll_content()->AddChildView(property_view);
        property_map_[property_view] = property_list[i].key;
      }

      // Adds a separator on the bottom of property items if there are still
      // other IMEs under the current one.
      if (i < list.size() - 1)
        scroll_content()->AddChildView(
            TrayPopupUtils::CreateListItemSeparator(true));
    }
  }
}

void ImeListView::AppendKeyboardStatus() {
  DCHECK(!MaterialDesignController::IsSystemTrayMenuMaterial());
  HoverHighlightView* container = new HoverHighlightView(this);
  int id = keyboard::IsKeyboardEnabled() ? IDS_ASH_STATUS_TRAY_DISABLE_KEYBOARD
                                         : IDS_ASH_STATUS_TRAY_ENABLE_KEYBOARD;
  container->AddLabel(
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(id),
      gfx::ALIGN_LEFT, false /* highlight */);
  scroll_content()->AddChildView(container);
  keyboard_status_ = container;
}

void ImeListView::PrependMaterialKeyboardStatus() {
  DCHECK(MaterialDesignController::IsSystemTrayMenuMaterial());
  DCHECK(!material_keyboard_status_view_);
  MaterialKeyboardStatusRowView* view =
      new MaterialKeyboardStatusRowView(this, keyboard::IsKeyboardEnabled());
  scroll_content()->AddChildViewAt(view, 0);
  material_keyboard_status_view_ = view;
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

void ImeListView::HandleButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (material_keyboard_status_view_ &&
      sender == material_keyboard_status_view_->toggle()) {
    WmShell::Get()->ToggleIgnoreExternalKeyboard();
  }
}

}  // namespace ash
