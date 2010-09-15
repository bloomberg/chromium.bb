// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

#ifndef CHROME_COMMON_WORKER_MESSAGES_H_
#define CHROME_COMMON_WORKER_MESSAGES_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_message_utils.h"

typedef std::pair<string16, std::vector<int> > QueuedMessage;

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
struct WorkerHostMsg_PostConsoleMessageToWorkerObject_Params {
  WorkerHostMsg_PostConsoleMessageToWorkerObject_Params();
  ~WorkerHostMsg_PostConsoleMessageToWorkerObject_Params();

  int source_identifier;
  int message_type;
  int message_level;
  string16 message;
  int line_number;
  string16 source_url;
};

// Parameter structure for WorkerProcessMsg_CreateWorker.
struct WorkerProcessMsg_CreateWorker_Params {
  WorkerProcessMsg_CreateWorker_Params();
  ~WorkerProcessMsg_CreateWorker_Params();

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
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

// Traits for WorkerProcessMsg_CreateWorker_Params.
template <>
struct ParamTraits<WorkerProcessMsg_CreateWorker_Params> {
  typedef WorkerProcessMsg_CreateWorker_Params param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/worker_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_WORKER_MESSAGES_H_
