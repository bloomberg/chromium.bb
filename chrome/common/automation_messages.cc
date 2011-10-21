// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/automation_messages.h"
#include "ui/base/models/menu_model.h"

ContextMenuModel::ContextMenuModel() {
}

ContextMenuModel::~ContextMenuModel() {
  for (size_t i = 0; i < items.size(); ++i)
    delete items[i].submenu;
}

ContextMenuModel::Item::Item()
    : type(static_cast<int>(ui::MenuModel::TYPE_COMMAND)),
      item_id(0),
      checked(false),
      enabled(true),
      submenu(NULL) {
}

namespace IPC {

// static
void ParamTraits<ContextMenuModel>::Write(Message* m,
                                          const param_type& p) {
  WriteParam(m, p.items.size());
  for (size_t i = 0; i < p.items.size(); ++i) {
    WriteParam(m, static_cast<int>(p.items[i].type));
    WriteParam(m, p.items[i].item_id);
    WriteParam(m, p.items[i].label);
    WriteParam(m, p.items[i].checked);
    WriteParam(m, p.items[i].enabled);

    if (p.items[i].type == static_cast<int>(ui::MenuModel::TYPE_SUBMENU)) {
      Write(m, *p.items[i].submenu);
    }
  }
}

// static
bool ParamTraits<ContextMenuModel>::Read(const Message* m,
                                         void** iter,
                                         param_type* p) {
  size_t item_count = 0;
  if (!ReadParam(m, iter, &item_count))
    return false;

  p->items.reserve(item_count);
  for (size_t i = 0; i < item_count; ++i) {
    ContextMenuModel::Item item;
    if (!ReadParam(m, iter, &item.type))
      return false;
    if (!ReadParam(m, iter, &item.item_id))
      return false;
    if (!ReadParam(m, iter, &item.label))
      return false;
    if (!ReadParam(m, iter, &item.checked))
      return false;
    if (!ReadParam(m, iter, &item.enabled))
      return false;

    if (item.type == static_cast<int>(ui::MenuModel::TYPE_SUBMENU)) {
      item.submenu = new ContextMenuModel;
      if (!Read(m, iter, item.submenu)) {
        delete item.submenu;
        item.submenu = NULL;
        return false;
      }
    }

    p->items.push_back(item);
  }

  return true;
}

// static
void ParamTraits<ContextMenuModel>::Log(const param_type& p,
                                        std::string* l) {
  l->append("(");
  for (size_t i = 0; i < p.items.size(); ++i) {
    const ContextMenuModel::Item& item = p.items[i];
    if (i)
      l->append(", ");
    l->append("(");
    LogParam(item.type, l);
    l->append(", ");
    LogParam(item.item_id, l);
    l->append(", ");
    LogParam(item.label, l);
    l->append(", ");
    LogParam(item.checked, l);
    l->append(", ");
    LogParam(item.enabled, l);
    if (item.type == ui::MenuModel::TYPE_SUBMENU) {
      l->append(", ");
      Log(*item.submenu, l);
    }
    l->append(")");
  }
  l->append(")");
}

}  // namespace IPC
