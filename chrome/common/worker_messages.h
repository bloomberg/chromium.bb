// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

#ifndef CHROME_COMMON_WORKER_MESSAGES_H_
#define CHROME_COMMON_WORKER_MESSAGES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/common/common_param_traits.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"

typedef std::pair<string16, std::vector<int> > QueuedMessage;

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
struct WorkerHostMsg_PostConsoleMessageToWorkerObject_Params {
  int source_identifier;
  int message_type;
  int message_level;
  string16 message;
  int line_number;
  string16 source_url;
};

// Parameter structure for WorkerProcessMsg_CreateWorker.
struct WorkerProcessMsg_CreateWorker_Params {
  GURL url;
  bool is_shared;
  string16 name;
  int route_id;
  int creator_process_id;
  int creator_appcache_host_id;  // Only valid for dedicated workers.
  int64 shared_worker_appcache_id;  // Only valid for shared workers.
};

namespace IPC {

// Traits for WorkerHostMsg_PostConsoleMessageToWorkerObject_Params structure
// to pack/unpack.
template <>
struct ParamTraits<WorkerHostMsg_PostConsoleMessageToWorkerObject_Params> {
  typedef WorkerHostMsg_PostConsoleMessageToWorkerObject_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.source_identifier);
    WriteParam(m, p.message_type);
    WriteParam(m, p.message_level);
    WriteParam(m, p.message);
    WriteParam(m, p.line_number);
    WriteParam(m, p.source_url);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->source_identifier) &&
      ReadParam(m, iter, &p->message_type) &&
      ReadParam(m, iter, &p->message_level) &&
      ReadParam(m, iter, &p->message) &&
      ReadParam(m, iter, &p->line_number) &&
      ReadParam(m, iter, &p->source_url);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.source_identifier, l);
    l->append(L", ");
    LogParam(p.message_type, l);
    l->append(L", ");
    LogParam(p.message_level, l);
    l->append(L", ");
    LogParam(p.message, l);
    l->append(L", ");
    LogParam(p.line_number, l);
    l->append(L", ");
    LogParam(p.source_url, l);
    l->append(L")");
  }
};

// Traits for WorkerProcessMsg_CreateWorker_Params.
template <>
struct ParamTraits<WorkerProcessMsg_CreateWorker_Params> {
  typedef WorkerProcessMsg_CreateWorker_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.url);
    WriteParam(m, p.is_shared);
    WriteParam(m, p.name);
    WriteParam(m, p.route_id);
    WriteParam(m, p.creator_process_id);
    WriteParam(m, p.creator_appcache_host_id);
    WriteParam(m, p.shared_worker_appcache_id);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->url) &&
      ReadParam(m, iter, &p->is_shared) &&
      ReadParam(m, iter, &p->name) &&
      ReadParam(m, iter, &p->route_id) &&
      ReadParam(m, iter, &p->creator_process_id) &&
      ReadParam(m, iter, &p->creator_appcache_host_id) &&
      ReadParam(m, iter, &p->shared_worker_appcache_id);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.url, l);
    l->append(L", ");
    LogParam(p.is_shared, l);
    l->append(L", ");
    LogParam(p.name, l);
    l->append(L", ");
    LogParam(p.route_id, l);
    l->append(L", ");
    LogParam(p.creator_process_id, l);
    l->append(L", ");
    LogParam(p.creator_appcache_host_id, l);
    l->append(L", ");
    LogParam(p.shared_worker_appcache_id, l);
    l->append(L")");
  }
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/worker_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_WORKER_MESSAGES_H_
