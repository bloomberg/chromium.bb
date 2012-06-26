// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/webkit_param_traits.h"
#include "ui/base/models/menu_model.h"

// Get basic type definitions.
#define IPC_MESSAGE_IMPL
#include "chrome/common/automation_messages.h"

// Generate constructors.
#include "ipc/struct_constructor_macros.h"
#include "chrome/common/automation_messages.h"

// Generate destructors.
#include "ipc/struct_destructor_macros.h"
#include "chrome/common/automation_messages.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "chrome/common/automation_messages.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "chrome/common/automation_messages.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "chrome/common/automation_messages.h"
}  // namespace IPC

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
                                         PickleIterator* iter,
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

// static
void ParamTraits<AutomationMouseEvent>::Write(Message* m,
                                              const param_type& p) {
  WriteParam(m, std::string(reinterpret_cast<const char*>(&p.mouse_event),
                            sizeof(p.mouse_event)));
  WriteParam(m, p.location_script_chain);
}

// static
bool ParamTraits<AutomationMouseEvent>::Read(const Message* m,
                                             PickleIterator* iter,
                                             param_type* p) {
  std::string mouse_event;
  if (!ReadParam(m, iter, &mouse_event))
    return false;
  memcpy(&p->mouse_event, mouse_event.c_str(), mouse_event.length());
  if (!ReadParam(m, iter, &p->location_script_chain))
    return false;
  return true;
}

// static
void ParamTraits<AutomationMouseEvent>::Log(const param_type& p,
                                            std::string* l) {
  l->append("(");
  LogParam(std::string(reinterpret_cast<const char*>(&p.mouse_event),
                       sizeof(p.mouse_event)),
           l);
  l->append(", ");
  LogParam(p.location_script_chain, l);
  l->append(")");
}

}  // namespace IPC
