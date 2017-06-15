// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/ime_menu/ime_list_view.h"

#include "ash/ime/ime_controller.h"
#include "ash/ime/ime_switch_type.h"
#include "ash/public/interfaces/ime_info.mojom.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/metrics/histogram_macros.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using chromeos::input_method::InputMethodManager;

namespace ash {
namespace {

const int kMinFontSizeDelta = -10;

// Represents a row in the scrollable IME list; each row is either an IME or
// an IME property. A checkmark icon is shown in the row if selected.
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
    SetInkDropMode(InkDropHostView::InkDropMode::ON);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);
    SetLayoutManager(new views::FillLayout);

    // |id_label| contains the IME short name (e.g., 'US', 'GB', 'IT').
    views::Label* id_label = TrayPopupUtils::CreateDefaultLabel();
    id_label->SetText(id);
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::FontList& base_font_list =
        rb.GetFontList(ui::ResourceBundle::MediumBoldFont);
    id_label->SetFontList(base_font_list);

    // IMEs having a short name of more than two characters (e.g., 'INTL') will
    // elide if rendered within |kMenuIconSize|. Shrink the font size until the
    // entire short name fits within the bounds.
    int size_delta = -1;
    while ((id_label->GetPreferredSize().width() -
            id_label->GetInsets().width()) > kMenuIconSize &&
           size_delta >= kMinFontSizeDelta) {
      id_label->SetFontList(base_font_list.DeriveWithSizeDelta(size_delta));
      --size_delta;
    }
    tri_view->AddView(TriView::Container::START, id_label);

    // The label shows the IME full name.
    auto* label_view = TrayPopupUtils::CreateDefaultLabel();
    label_view->SetText(label);
    TrayPopupItemStyle style(
        TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(label_view);

    label_view->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    tri_view->AddView(TriView::Container::CENTER, label_view);

    if (selected) {
      // The checked button indicates the IME is selected.
      views::ImageView* checked_image = TrayPopupUtils::CreateMainImageView();
      checked_image->SetImage(
          gfx::CreateVectorIcon(kCheckCircleIcon, kMenuIconSize, button_color));
      tri_view->AddView(TriView::Container::END, checked_image);
    }
    SetAccessibleName(label_view->text());
  }

  ~ImeListItemView() override {}

  // ActionableView:
  bool PerformAction(const ui::Event& event) override {
    ime_list_view_->set_last_item_selected_with_keyboard(
        ime_list_view_->should_focus_ime_after_selection_with_keyboard() &&
        event.type() == ui::EventType::ET_KEY_PRESSED);
    ime_list_view_->HandleViewClicked(this);
    return true;
  }

  void OnFocus() override {
    ActionableView::OnFocus();
    if (ime_list_view_)
      ime_list_view_->ScrollItemToVisible(this);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    ActionableView::GetAccessibleNodeData(node_data);
    node_data->role = ui::AX_ROLE_CHECK_BOX;
    const ui::AXCheckedState checked_state =
        selected_ ? ui::AX_CHECKED_STATE_TRUE : ui::AX_CHECKED_STATE_FALSE;
    node_data->AddIntAttribute(ui::AX_ATTR_CHECKED_STATE, checked_state);
  }

 private:
  ImeListView* ime_list_view_;
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(ImeListItemView);
};

}  // namespace

// Contains a toggle button to let the user enable/disable whether the
// on-screen keyboard should be shown when focusing a textfield. This row is
// shown only under certain conditions, e.g., when an external keyboard is
// attached and the user is in TouchView mode.
class KeyboardStatusRow : public views::View {
 public:
  KeyboardStatusRow() {}
  ~KeyboardStatusRow() override {}

  views::ToggleButton* toggle() const { return toggle_; }
  bool is_toggled() const { return toggle_->is_on(); }

  void Init(views::ButtonListener* listener) {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(new views::FillLayout);

    TriView* tri_view = TrayPopupUtils::CreateDefaultRowView();
    AddChildView(tri_view);

    // The on-screen keyboard image button.
    views::ImageView* keyboard_image = TrayPopupUtils::CreateMainImageView();
    keyboard_image->SetImage(gfx::CreateVectorIcon(
        kImeMenuOnScreenKeyboardIcon, kMenuIconSize, kMenuIconColor));
    tri_view->AddView(TriView::Container::START, keyboard_image);

    // The on-screen keyboard label ('On-screen keyboard').
    auto* label = TrayPopupUtils::CreateDefaultLabel();
    label->SetText(ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
        IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD));
    TrayPopupItemStyle style(
        TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
    style.SetupLabel(label);
    tri_view->AddView(TriView::Container::CENTER, label);

    // The on-screen keyboard toggle button.
    toggle_ = TrayPopupUtils::CreateToggleButton(
        listener, IDS_ASH_STATUS_TRAY_ACCESSIBILITY_VIRTUAL_KEYBOARD);
    toggle_->SetIsOn(keyboard::IsKeyboardEnabled(), false);
    tri_view->AddView(TriView::Container::END, toggle_);
  }

 private:
  // ToggleButton to toggle keyboard on or off.
  views::ToggleButton* toggle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(KeyboardStatusRow);
};

ImeListView::ImeListView(SystemTrayItem* owner)
    : TrayDetailsView(owner),
      last_item_selected_with_keyboard_(false),
      should_focus_ime_after_selection_with_keyboard_(false),
      current_ime_view_(nullptr) {}

ImeListView::~ImeListView() {}

void ImeListView::Init(bool show_keyboard_toggle,
                       SingleImeBehavior single_ime_behavior) {
  ImeController* ime_controller = Shell::Get()->ime_controller();
  Update(ime_controller->available_imes(),
         ime_controller->current_ime_menu_items(), show_keyboard_toggle,
         single_ime_behavior);
}

void ImeListView::Update(const std::vector<mojom::ImeInfo>& list,
                         const std::vector<mojom::ImeMenuItem>& property_items,
                         bool show_keyboard_toggle,
                         SingleImeBehavior single_ime_behavior) {
  ResetImeListView();
  ime_map_.clear();
  property_map_.clear();
  CreateScrollableList();

  if (single_ime_behavior == ImeListView::SHOW_SINGLE_IME || list.size() > 1)
    AppendImeListAndProperties(list, property_items);

  if (show_keyboard_toggle)
    PrependKeyboardStatusRow();

  Layout();
  SchedulePaint();

  if (should_focus_ime_after_selection_with_keyboard_ &&
      last_item_selected_with_keyboard_) {
    FocusCurrentImeIfNeeded();
  } else if (current_ime_view_) {
    ScrollItemToVisible(current_ime_view_);
  }
}

void ImeListView::ResetImeListView() {
  // Children are removed from the view hierarchy and deleted in Reset().
  Reset();
  keyboard_status_row_ = nullptr;
  current_ime_view_ = nullptr;
}

void ImeListView::ScrollItemToVisible(views::View* item_view) {
  if (scroll_content())
    scroll_content()->ScrollRectToVisible(item_view->bounds());
}

void ImeListView::CloseImeListView() {
  last_selected_item_id_.clear();
  current_ime_view_ = nullptr;
  last_item_selected_with_keyboard_ = false;
  GetWidget()->Close();
}

void ImeListView::AppendImeListAndProperties(
    const std::vector<mojom::ImeInfo>& list,
    const std::vector<mojom::ImeMenuItem>& property_list) {
  DCHECK(ime_map_.empty());
  for (size_t i = 0; i < list.size(); i++) {
    views::View* ime_view =
        new ImeListItemView(owner(), this, list[i].short_name, list[i].name,
                            list[i].selected, gfx::kGoogleGreen700);
    scroll_content()->AddChildView(ime_view);
    ime_map_[ime_view] = list[i].id;

    if (list[i].selected)
      current_ime_view_ = ime_view;

    // Add the properties, if any, of the currently-selected IME.
    if (list[i].selected && !property_list.empty()) {
      // Adds a separator on the top of property items.
      scroll_content()->AddChildView(
          TrayPopupUtils::CreateListItemSeparator(true));

      // Adds the property items.
      for (size_t i = 0; i < property_list.size(); i++) {
        ImeListItemView* property_view = new ImeListItemView(
            owner(), this, base::string16(), property_list[i].label,
            property_list[i].checked, kMenuIconColor);
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

void ImeListView::PrependKeyboardStatusRow() {
  DCHECK(!keyboard_status_row_);
  keyboard_status_row_ = new KeyboardStatusRow;
  keyboard_status_row_->Init(this);
  scroll_content()->AddChildViewAt(keyboard_status_row_, 0);
}

void ImeListView::HandleViewClicked(views::View* view) {
  std::map<views::View*, std::string>::const_iterator ime = ime_map_.find(view);
  if (ime != ime_map_.end()) {
    ShellPort::Get()->RecordUserMetricsAction(UMA_STATUS_AREA_IME_SWITCH_MODE);
    std::string ime_id = ime->second;
    last_selected_item_id_ = ime_id;
    InputMethodManager::Get()->GetActiveIMEState()->ChangeInputMethod(
        ime_id, false /* show_message */);
    UMA_HISTOGRAM_ENUMERATION("InputMethod.ImeSwitch", ImeSwitchType::kTray,
                              ImeSwitchType::kCount);

  } else {
    std::map<views::View*, std::string>::const_iterator property =
        property_map_.find(view);
    if (property == property_map_.end())
      return;
    const std::string key = property->second;
    last_selected_item_id_ = key;
    InputMethodManager::Get()->ActivateInputMethodMenuItem(key);
  }

  if (!should_focus_ime_after_selection_with_keyboard_ ||
      !last_item_selected_with_keyboard_) {
    CloseImeListView();
  }
}

void ImeListView::HandleButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  DCHECK_EQ(sender, keyboard_status_row_->toggle());

  ShellPort::Get()->ToggleIgnoreExternalKeyboard();
  last_selected_item_id_.clear();
  last_item_selected_with_keyboard_ = false;
}

void ImeListView::VisibilityChanged(View* starting_from, bool is_visible) {
  if (!is_visible || (should_focus_ime_after_selection_with_keyboard_ &&
                      last_item_selected_with_keyboard_) ||
      !current_ime_view_) {
    return;
  }

  ScrollItemToVisible(current_ime_view_);
}

void ImeListView::FocusCurrentImeIfNeeded() {
  views::FocusManager* manager = GetFocusManager();
  if (!manager || manager->GetFocusedView() || last_selected_item_id_.empty())
    return;

  for (auto ime_map : ime_map_) {
    if (ime_map.second == last_selected_item_id_) {
      (ime_map.first)->RequestFocus();
      return;
    }
  }

  for (auto property_map : property_map_) {
    if (property_map.second == last_selected_item_id_) {
      (property_map.first)->RequestFocus();
      return;
    }
  }
}

ImeListViewTestApi::ImeListViewTestApi(ImeListView* ime_list_view)
    : ime_list_view_(ime_list_view) {}

ImeListViewTestApi::~ImeListViewTestApi() {}

views::View* ImeListViewTestApi::GetToggleView() const {
  return ime_list_view_->keyboard_status_row_->toggle();
}

}  // namespace ash
