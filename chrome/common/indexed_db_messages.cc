// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/common_param_traits.h"

#define IPC_MESSAGE_IMPL
#include "chrome/common/indexed_db_messages.h"

IndexedDBHostMsg_FactoryOpen_Params::IndexedDBHostMsg_FactoryOpen_Params()
    : routing_id(0),
      response_id(0),
      maximum_size(0) {
}

IndexedDBHostMsg_FactoryOpen_Params::~IndexedDBHostMsg_FactoryOpen_Params() {
}

IndexedDBHostMsg_DatabaseCreateObjectStore_Params::
    IndexedDBHostMsg_DatabaseCreateObjectStore_Params()
    : auto_increment(false),
      transaction_id(0),
      idb_database_id(0) {
}

IndexedDBHostMsg_DatabaseCreateObjectStore_Params::
    ~IndexedDBHostMsg_DatabaseCreateObjectStore_Params() {
}

IndexedDBHostMsg_IndexOpenCursor_Params::
    IndexedDBHostMsg_IndexOpenCursor_Params()
    : response_id(0),
      lower_open(false),
      upper_open(false),
      direction(0),
      idb_index_id(0),
      transaction_id(0) {
}

IndexedDBHostMsg_IndexOpenCursor_Params::
    ~IndexedDBHostMsg_IndexOpenCursor_Params() {
}


IndexedDBHostMsg_ObjectStorePut_Params::
    IndexedDBHostMsg_ObjectStorePut_Params()
    : idb_object_store_id(0),
      response_id(0),
      put_mode(),
      transaction_id(0) {
}

IndexedDBHostMsg_ObjectStorePut_Params::
~IndexedDBHostMsg_ObjectStorePut_Params() {
}

IndexedDBHostMsg_ObjectStoreCreateIndex_Params::
    IndexedDBHostMsg_ObjectStoreCreateIndex_Params()
    : unique(false),
      transaction_id(0),
      idb_object_store_id(0) {
}

IndexedDBHostMsg_ObjectStoreCreateIndex_Params::
    ~IndexedDBHostMsg_ObjectStoreCreateIndex_Params() {
}


IndexedDBHostMsg_ObjectStoreOpenCursor_Params::
    IndexedDBHostMsg_ObjectStoreOpenCursor_Params()
    : response_id(0),
      lower_open(false),
      upper_open(false),
      direction(0),
      idb_object_store_id(0),
      transaction_id(0) {
}

IndexedDBHostMsg_ObjectStoreOpenCursor_Params::
    ~IndexedDBHostMsg_ObjectStoreOpenCursor_Params() {
}

namespace IPC {

void ParamTraits<IndexedDBHostMsg_FactoryOpen_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.routing_id);
  WriteParam(m, p.response_id);
  WriteParam(m, p.origin);
  WriteParam(m, p.name);
  WriteParam(m, p.maximum_size);
}

bool ParamTraits<IndexedDBHostMsg_FactoryOpen_Params>::Read(const Message* m,
                                                            void** iter,
                                                            param_type* p) {
  return
      ReadParam(m, iter, &p->routing_id) &&
      ReadParam(m, iter, &p->response_id) &&
      ReadParam(m, iter, &p->origin) &&
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->maximum_size);
}

void ParamTraits<IndexedDBHostMsg_FactoryOpen_Params>::Log(const param_type& p,
                                                           std::string* l) {
  l->append("(");
  LogParam(p.routing_id, l);
  l->append(", ");
  LogParam(p.response_id, l);
  l->append(", ");
  LogParam(p.origin, l);
  l->append(", ");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.maximum_size, l);
  l->append(")");
}

void ParamTraits<IndexedDBHostMsg_DatabaseCreateObjectStore_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.key_path);
  WriteParam(m, p.auto_increment);
  WriteParam(m, p.transaction_id);
  WriteParam(m, p.idb_database_id);
}

bool ParamTraits<IndexedDBHostMsg_DatabaseCreateObjectStore_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->key_path) &&
      ReadParam(m, iter, &p->auto_increment) &&
      ReadParam(m, iter, &p->transaction_id) &&
      ReadParam(m, iter, &p->idb_database_id);
}

void ParamTraits<IndexedDBHostMsg_DatabaseCreateObjectStore_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.key_path, l);
  l->append(", ");
  LogParam(p.auto_increment, l);
  l->append(", ");
  LogParam(p.transaction_id, l);
  l->append(", ");
  LogParam(p.idb_database_id, l);
  l->append(")");
}

void ParamTraits<IndexedDBHostMsg_IndexOpenCursor_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.response_id);
  WriteParam(m, p.lower_key);
  WriteParam(m, p.upper_key);
  WriteParam(m, p.lower_open);
  WriteParam(m, p.upper_open);
  WriteParam(m, p.direction);
  WriteParam(m, p.idb_index_id);
  WriteParam(m, p.transaction_id);
}

bool ParamTraits<IndexedDBHostMsg_IndexOpenCursor_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->response_id) &&
      ReadParam(m, iter, &p->lower_key) &&
      ReadParam(m, iter, &p->upper_key) &&
      ReadParam(m, iter, &p->lower_open) &&
      ReadParam(m, iter, &p->upper_open) &&
      ReadParam(m, iter, &p->direction) &&
      ReadParam(m, iter, &p->idb_index_id) &&
      ReadParam(m, iter, &p->transaction_id);
}

void ParamTraits<IndexedDBHostMsg_IndexOpenCursor_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.response_id, l);
  l->append(", ");
  LogParam(p.lower_key, l);
  l->append(", ");
  LogParam(p.upper_key, l);
  l->append(", ");
  LogParam(p.lower_open, l);
  l->append(", ");
  LogParam(p.upper_open, l);
  l->append(", ");
  LogParam(p.direction, l);
  l->append(", ");
  LogParam(p.idb_index_id, l);
  l->append(",");
  LogParam(p.transaction_id, l);
  l->append(")");
}

void ParamTraits<IndexedDBHostMsg_ObjectStorePut_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.idb_object_store_id);
  WriteParam(m, p.response_id);
  WriteParam(m, p.serialized_value);
  WriteParam(m, p.key);
  WriteParam(m, p.put_mode);
  WriteParam(m, p.transaction_id);
}

bool ParamTraits<IndexedDBHostMsg_ObjectStorePut_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->idb_object_store_id) &&
      ReadParam(m, iter, &p->response_id) &&
      ReadParam(m, iter, &p->serialized_value) &&
      ReadParam(m, iter, &p->key) &&
      ReadParam(m, iter, &p->put_mode) &&
      ReadParam(m, iter, &p->transaction_id);
}

void ParamTraits<IndexedDBHostMsg_ObjectStorePut_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.idb_object_store_id, l);
  l->append(", ");
  LogParam(p.response_id, l);
  l->append(", ");
  LogParam(p.serialized_value, l);
  l->append(", ");
  LogParam(p.key, l);
  l->append(", ");
  LogParam(p.put_mode, l);
  l->append(", ");
  LogParam(p.transaction_id, l);
  l->append(")");
}

void ParamTraits<IndexedDBHostMsg_ObjectStoreCreateIndex_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.name);
  WriteParam(m, p.key_path);
  WriteParam(m, p.unique);
  WriteParam(m, p.transaction_id);
  WriteParam(m, p.idb_object_store_id);
}

bool ParamTraits<IndexedDBHostMsg_ObjectStoreCreateIndex_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->key_path) &&
      ReadParam(m, iter, &p->unique) &&
      ReadParam(m, iter, &p->transaction_id) &&
      ReadParam(m, iter, &p->idb_object_store_id);
}

void ParamTraits<IndexedDBHostMsg_ObjectStoreCreateIndex_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.key_path, l);
  l->append(", ");
  LogParam(p.unique, l);
  l->append(", ");
  LogParam(p.transaction_id, l);
  l->append(", ");
  LogParam(p.idb_object_store_id, l);
  l->append(")");
}

void ParamTraits<IndexedDBHostMsg_ObjectStoreOpenCursor_Params>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, p.response_id);
  WriteParam(m, p.lower_key);
  WriteParam(m, p.upper_key);
  WriteParam(m, p.lower_open);
  WriteParam(m, p.upper_open);
  WriteParam(m, p.direction);
  WriteParam(m, p.idb_object_store_id);
  WriteParam(m, p.transaction_id);
}

bool ParamTraits<IndexedDBHostMsg_ObjectStoreOpenCursor_Params>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  return
      ReadParam(m, iter, &p->response_id) &&
      ReadParam(m, iter, &p->lower_key) &&
      ReadParam(m, iter, &p->upper_key) &&
      ReadParam(m, iter, &p->lower_open) &&
      ReadParam(m, iter, &p->upper_open) &&
      ReadParam(m, iter, &p->direction) &&
      ReadParam(m, iter, &p->idb_object_store_id) &&
      ReadParam(m, iter, &p->transaction_id);
}

void ParamTraits<IndexedDBHostMsg_ObjectStoreOpenCursor_Params>::Log(
    const param_type& p,
    std::string* l) {
  l->append("(");
  LogParam(p.response_id, l);
  l->append(", ");
  LogParam(p.lower_key, l);
  l->append(", ");
  LogParam(p.upper_key, l);
  l->append(", ");
  LogParam(p.lower_open, l);
  l->append(", ");
  LogParam(p.upper_open, l);
  l->append(", ");
  LogParam(p.direction, l);
  l->append(", ");
  LogParam(p.idb_object_store_id, l);
  l->append(",");
  LogParam(p.transaction_id, l);
  l->append(")");
}

void ParamTraits<WebKit::WebIDBObjectStore::PutMode>::Write(
    Message* m,
    const param_type& p) {
  WriteParam(m, static_cast<int>(p));
}

bool ParamTraits<WebKit::WebIDBObjectStore::PutMode>::Read(
    const Message* m,
    void** iter,
    param_type* p) {
  int i;
  bool ok = ReadParam(m, iter, &i);
  if (!ok)
    i = 0;
  *p = static_cast<param_type>(i);
  return ok;
}

void ParamTraits<WebKit::WebIDBObjectStore::PutMode>::Log(
    const param_type& p,
    std::string* l) {
  LogParam(static_cast<int>(p), l);
}

}  // namespace IPC
