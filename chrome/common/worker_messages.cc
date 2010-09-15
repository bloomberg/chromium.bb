// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/worker_messages.h"

#include "chrome/common/common_param_traits.h"

#define MESSAGES_INTERNAL_IMPL_FILE \
  "chrome/common/worker_messages_internal.h"
#include "ipc/ipc_message_impl_macros.h"

WorkerHostMsg_PostConsoleMessageToWorkerObject_Params::
WorkerHostMsg_PostConsoleMessageToWorkerObject_Params()
    : source_identifier(-1),
      message_type(-1),
      message_level(-1),
      line_number(-1) {
}

WorkerHostMsg_PostConsoleMessageToWorkerObject_Params::
~WorkerHostMsg_PostConsoleMessageToWorkerObject_Params() {
}

WorkerProcessMsg_CreateWorker_Params::WorkerProcessMsg_CreateWorker_Params()
    : is_shared(false),
      route_id(-1),
      creator_process_id(-1),
      creator_appcache_host_id(-1),
      shared_worker_appcache_id(-1) {
}

WorkerProcessMsg_CreateWorker_Params::~WorkerProcessMsg_CreateWorker_Params() {
}


namespace IPC {

void ParamTraits<WorkerHostMsg_PostConsoleMessageToWorkerObject_Params>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.source_identifier);
  WriteParam(m, p.message_type);
  WriteParam(m, p.message_level);
  WriteParam(m, p.message);
  WriteParam(m, p.line_number);
  WriteParam(m, p.source_url);
}

bool ParamTraits<WorkerHostMsg_PostConsoleMessageToWorkerObject_Params>::Read(
    const Message* m, void** iter, param_type* p) {
  return
      ReadParam(m, iter, &p->source_identifier) &&
      ReadParam(m, iter, &p->message_type) &&
      ReadParam(m, iter, &p->message_level) &&
      ReadParam(m, iter, &p->message) &&
      ReadParam(m, iter, &p->line_number) &&
      ReadParam(m, iter, &p->source_url);
}

void ParamTraits<WorkerHostMsg_PostConsoleMessageToWorkerObject_Params>::Log(
    const param_type& p, std::string* l) {
  l->append("(");
  LogParam(p.source_identifier, l);
  l->append(", ");
  LogParam(p.message_type, l);
  l->append(", ");
  LogParam(p.message_level, l);
  l->append(", ");
  LogParam(p.message, l);
  l->append(", ");
  LogParam(p.line_number, l);
  l->append(", ");
  LogParam(p.source_url, l);
  l->append(")");
}

void ParamTraits<WorkerProcessMsg_CreateWorker_Params>::Write(
    Message* m, const param_type& p) {
  WriteParam(m, p.url);
  WriteParam(m, p.is_shared);
  WriteParam(m, p.name);
  WriteParam(m, p.route_id);
  WriteParam(m, p.creator_process_id);
  WriteParam(m, p.creator_appcache_host_id);
  WriteParam(m, p.shared_worker_appcache_id);
}

bool ParamTraits<WorkerProcessMsg_CreateWorker_Params>::Read(const Message* m,
                                                             void** iter,
                                                             param_type* p) {
  return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->is_shared) &&
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->route_id) &&
      ReadParam(m, iter, &p->creator_process_id) &&
      ReadParam(m, iter, &p->creator_appcache_host_id) &&
      ReadParam(m, iter, &p->shared_worker_appcache_id);
}

void ParamTraits<WorkerProcessMsg_CreateWorker_Params>::Log(const param_type& p,
                                                            std::string* l) {
  l->append("(");
  LogParam(p.url, l);
  l->append(", ");
  LogParam(p.is_shared, l);
  l->append(", ");
  LogParam(p.name, l);
  l->append(", ");
  LogParam(p.route_id, l);
  l->append(", ");
  LogParam(p.creator_process_id, l);
  l->append(", ");
  LogParam(p.creator_appcache_host_id, l);
  l->append(", ");
  LogParam(p.shared_worker_appcache_id, l);
  l->append(")");
}

}  // namespace IPC
