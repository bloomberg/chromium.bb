// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "app/menus/simple_menu_model.h"

#include "app/l10n_util.h"

namespace menus {

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, public:

SimpleMenuModel::SimpleMenuModel(Delegate* delegate) : delegate_(delegate) {
}

SimpleMenuModel::~SimpleMenuModel() {
}

void SimpleMenuModel::AddItem(int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_COMMAND, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddItemWithStringId(int command_id, int string_id) {
  AddItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddSeparator() {
  Item item = { -1, string16(), TYPE_SEPARATOR, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddCheckItem(int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_CHECK, -1, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddCheckItemWithStringId(int command_id, int string_id) {
  AddCheckItem(command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::AddRadioItem(int command_id, const string16& label,
                                   int group_id) {
  Item item = { command_id, label, TYPE_RADIO, group_id, NULL };
  items_.push_back(item);
}

void SimpleMenuModel::AddRadioItemWithStringId(int command_id, int string_id,
                                               int group_id) {
  AddRadioItem(command_id, l10n_util::GetStringUTF16(string_id), group_id);
}

void SimpleMenuModel::AddSubMenu(const string16& label, MenuModel* model) {
  Item item = { -1, label, TYPE_SUBMENU, -1, model };
  items_.push_back(item);
}

void SimpleMenuModel::AddSubMenuWithStringId(int string_id, MenuModel* model) {
  AddSubMenu(l10n_util::GetStringUTF16(string_id), model);
}

void SimpleMenuModel::InsertItemAt(
    int index, int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_COMMAND, -1, NULL };
  items_.insert(items_.begin() + FlipIndex(index), item);
}

void SimpleMenuModel::InsertItemWithStringIdAt(
    int index, int command_id, int string_id) {
  InsertItemAt(index, command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::InsertSeparatorAt(int index) {
  Item item = { -1, string16(), TYPE_SEPARATOR, -1, NULL };
  items_.insert(items_.begin() + FlipIndex(index), item);
}

void SimpleMenuModel::InsertCheckItemAt(
    int index, int command_id, const string16& label) {
  Item item = { command_id, label, TYPE_CHECK, -1, NULL };
  items_.insert(items_.begin() + FlipIndex(index), item);
}

void SimpleMenuModel::InsertCheckItemWithStringIdAt(
    int index, int command_id, int string_id) {
  InsertCheckItemAt(
      FlipIndex(index), command_id, l10n_util::GetStringUTF16(string_id));
}

void SimpleMenuModel::InsertRadioItemAt(
    int index, int command_id, const string16& label, int group_id) {
  Item item = { command_id, label, TYPE_RADIO, group_id, NULL };
  items_.insert(items_.begin() + FlipIndex(index), item);
}

void SimpleMenuModel::InsertRadioItemWithStringIdAt(
    int index, int command_id, int string_id, int group_id) {
  InsertRadioItemAt(
      index, command_id, l10n_util::GetStringUTF16(string_id), group_id);
}

void SimpleMenuModel::InsertSubMenuAt(
    int index, const string16& label, MenuModel* model) {
  Item item = { -1, label, TYPE_SUBMENU, -1, model };
  items_.insert(items_.begin() + FlipIndex(index), item);
}

void SimpleMenuModel::InsertSubMenuWithStringIdAt(
    int index, int string_id, MenuModel* model) {
  InsertSubMenuAt(index, l10n_util::GetStringUTF16(string_id), model);
}

int SimpleMenuModel::GetIndexOfCommandId(int command_id) {
  std::vector<Item>::iterator itr;
  for (itr = items_.begin(); itr != items_.end(); itr++) {
    if ((*itr).command_id == command_id) {
      return FlipIndex(static_cast<int>(std::distance(items_.begin(), itr)));
    }
  }
  return -1;
}

////////////////////////////////////////////////////////////////////////////////
// SimpleMenuModel, MenuModel implementation:

bool SimpleMenuModel::HasIcons() const {
  return false;
}

int SimpleMenuModel::GetItemCount() const {
  return static_cast<int>(items_.size());
}

MenuModel::ItemType SimpleMenuModel::GetTypeAt(int index) const {
  return items_.at(FlipIndex(index)).type;
}

int SimpleMenuModel::GetCommandIdAt(int index) const {
  return items_.at(FlipIndex(index)).command_id;
}

string16 SimpleMenuModel::GetLabelAt(int index) const {
  if (IsLabelDynamicAt(index))
    return delegate_->GetLabelForCommandId(GetCommandIdAt(index));
  return items_.at(FlipIndex(index)).label;
}

bool SimpleMenuModel::IsLabelDynamicAt(int index) const {
  if (delegate_)
    return delegate_->IsLabelForCommandIdDynamic(GetCommandIdAt(index));
  return false;
}

bool SimpleMenuModel::GetAcceleratorAt(int index,
                                       menus::Accelerator* accelerator) const {
  if (delegate_) {
    return delegate_->GetAcceleratorForCommandId(GetCommandIdAt(index),
                                                 accelerator);
  }
  return false;
}

bool SimpleMenuModel::IsItemCheckedAt(int index) const {
  if (!delegate_)
    return false;
  int item_index = FlipIndex(index);
  MenuModel::ItemType item_type = items_[item_index].type;
  return (item_type == TYPE_CHECK || item_type == TYPE_RADIO) ?
      delegate_->IsCommandIdChecked(GetCommandIdAt(index)) : false;
}

int SimpleMenuModel::GetGroupIdAt(int index) const {
  return items_.at(FlipIndex(index)).group_id;
}

bool SimpleMenuModel::GetIconAt(int index, SkBitmap* icon) const {
  return false;
}

bool SimpleMenuModel::IsEnabledAt(int index) const {
  int command_id = GetCommandIdAt(index);
  // Submenus have a command id of -1, they should always be enabled.
  if (!delegate_ || command_id == -1)
    return true;
  return delegate_->IsCommandIdEnabled(command_id);
}

void SimpleMenuModel::HighlightChangedTo(int index) {
  if (delegate_)
    delegate_->CommandIdHighlighted(GetCommandIdAt(index));
}

void SimpleMenuModel::ActivatedAt(int index) {
  if (delegate_)
    delegate_->ExecuteCommand(GetCommandIdAt(index));
}

MenuModel* SimpleMenuModel::GetSubmenuModelAt(int index) const {
  return items_.at(FlipIndex(index)).submenu;
}

}  // namespace views
