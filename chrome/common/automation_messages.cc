// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

void ParamTraits<AutomationMouseEvent>::Write(Message* m,
                                              const param_type& p) {
  WriteParam(m, std::string(reinterpret_cast<const char*>(&p.mouse_event),
                            sizeof(p.mouse_event)));
  WriteParam(m, p.location_script_chain);
}

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

// Only the net::UploadData ParamTraits<> definition needs this definition, so
// keep this in the implementation file so we can forward declare UploadData in
// the header.
template <>
struct ParamTraits<net::UploadElement> {
  typedef net::UploadElement param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, static_cast<int>(p.type()));
    switch (p.type()) {
      case net::UploadElement::TYPE_BYTES: {
        m->WriteData(p.bytes(), static_cast<int>(p.bytes_length()));
        break;
      }
      default: {
        DCHECK(p.type() == net::UploadElement::TYPE_FILE);
        WriteParam(m, p.file_path());
        WriteParam(m, p.file_range_offset());
        WriteParam(m, p.file_range_length());
        WriteParam(m, p.expected_file_modification_time());
        break;
      }
    }
  }
  static bool Read(const Message* m, PickleIterator* iter, param_type* r) {
    int type;
    if (!ReadParam(m, iter, &type))
      return false;
    switch (type) {
      case net::UploadElement::TYPE_BYTES: {
        const char* data;
        int len;
        if (!m->ReadData(iter, &data, &len))
          return false;
        r->SetToBytes(data, len);
        break;
      }
      default: {
        DCHECK(type == net::UploadElement::TYPE_FILE);
        base::FilePath file_path;
        uint64 offset, length;
        base::Time expected_modification_time;
        if (!ReadParam(m, iter, &file_path))
          return false;
        if (!ReadParam(m, iter, &offset))
          return false;
        if (!ReadParam(m, iter, &length))
          return false;
        if (!ReadParam(m, iter, &expected_modification_time))
          return false;
        r->SetToFilePathRange(file_path, offset, length,
                              expected_modification_time);
        break;
      }
    }
    return true;
  }
  static void Log(const param_type& p, std::string* l) {
    l->append("<net::UploadElement>");
  }
};

void ParamTraits<scoped_refptr<net::UploadData> >::Write(Message* m,
                                                         const param_type& p) {
  WriteParam(m, p.get() != NULL);
  if (p) {
    WriteParam(m, p->elements());
    WriteParam(m, p->identifier());
    WriteParam(m, p->is_chunked());
    WriteParam(m, p->last_chunk_appended());
  }
}

bool ParamTraits<scoped_refptr<net::UploadData> >::Read(const Message* m,
                                                        PickleIterator* iter,
                                                        param_type* r) {
  bool has_object;
  if (!ReadParam(m, iter, &has_object))
    return false;
  if (!has_object)
    return true;
  ScopedVector<net::UploadElement> elements;
  if (!ReadParam(m, iter, &elements))
    return false;
  int64 identifier;
  if (!ReadParam(m, iter, &identifier))
    return false;
  bool is_chunked = false;
  if (!ReadParam(m, iter, &is_chunked))
    return false;
  bool last_chunk_appended = false;
  if (!ReadParam(m, iter, &last_chunk_appended))
    return false;
  *r = new net::UploadData;
  (*r)->swap_elements(&elements);
  (*r)->set_identifier(identifier);
  (*r)->set_is_chunked(is_chunked);
  (*r)->set_last_chunk_appended(last_chunk_appended);
  return true;
}

void ParamTraits<scoped_refptr<net::UploadData> >::Log(const param_type& p,
                                                       std::string* l) {
  l->append("<net::UploadData>");
}

void ParamTraits<net::URLRequestStatus>::Write(Message* m,
                                               const param_type& p) {
  WriteParam(m, static_cast<int>(p.status()));
  WriteParam(m, p.error());
}

bool ParamTraits<net::URLRequestStatus>::Read(const Message* m,
                                              PickleIterator* iter,
                                              param_type* r) {
  int status, error;
  if (!ReadParam(m, iter, &status) || !ReadParam(m, iter, &error))
    return false;
  r->set_status(static_cast<net::URLRequestStatus::Status>(status));
  r->set_error(error);
  return true;
}

void ParamTraits<net::URLRequestStatus>::Log(const param_type& p,
                                             std::string* l) {
  std::string status;
  switch (p.status()) {
    case net::URLRequestStatus::SUCCESS:
      status = "SUCCESS";
      break;
    case net::URLRequestStatus::IO_PENDING:
      status = "IO_PENDING ";
      break;
    case net::URLRequestStatus::CANCELED:
      status = "CANCELED";
      break;
    case net::URLRequestStatus::FAILED:
      status = "FAILED";
      break;
    default:
      status = "UNKNOWN";
      break;
  }
  if (p.status() == net::URLRequestStatus::FAILED)
    l->append("(");

  LogParam(status, l);

  if (p.status() == net::URLRequestStatus::FAILED) {
    l->append(", ");
    LogParam(p.error(), l);
    l->append(")");
  }
}

}  // namespace IPC
