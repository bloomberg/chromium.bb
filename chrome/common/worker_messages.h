// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines messages between the browser and worker process, as well as between
// the renderer and worker process.

#ifndef CHROME_COMMON_WORKER_MESSAGES_H_
#define CHROME_COMMON_WORKER_MESSAGES_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/common/common_param_traits.h"
#include "ipc/ipc_message_utils.h"

// Parameters structure for WorkerHostMsg_PostConsoleMessageToWorkerObject,
// which has too many data parameters to be reasonably put in a predefined
// IPC message. The data members directly correspond to parameters of
// WebWorkerClient::postConsoleMessageToWorkerObject()
struct WorkerHostMsg_PostConsoleMessageToWorkerObject_Params {
  int destination_identifier;
  int source_identifier;
  int message_type;
  int message_level;
  string16 message;
  int line_number;
  string16 source_url;
};

namespace IPC {

// Traits for WorkerHostMsg_PostConsoleMessageToWorkerObject_Params structure
// to pack/unpack.
template <>
struct ParamTraits<WorkerHostMsg_PostConsoleMessageToWorkerObject_Params> {
  typedef WorkerHostMsg_PostConsoleMessageToWorkerObject_Params param_type;
  static void Write(Message* m, const param_type& p) {
    WriteParam(m, p.destination_identifier);
    WriteParam(m, p.source_identifier);
    WriteParam(m, p.message_type);
    WriteParam(m, p.message_level);
    WriteParam(m, p.message);
    WriteParam(m, p.line_number);
    WriteParam(m, p.source_url);
  }
  static bool Read(const Message* m, void** iter, param_type* p) {
    return
      ReadParam(m, iter, &p->destination_identifier) &&
      ReadParam(m, iter, &p->source_identifier) &&
      ReadParam(m, iter, &p->message_type) &&
      ReadParam(m, iter, &p->message_level) &&
      ReadParam(m, iter, &p->message) &&
      ReadParam(m, iter, &p->line_number) &&
      ReadParam(m, iter, &p->source_url);
  }
  static void Log(const param_type& p, std::wstring* l) {
    l->append(L"(");
    LogParam(p.destination_identifier, l);
    l->append(L", ");
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

}  // namespace IPC

#define MESSAGES_INTERNAL_FILE "chrome/common/worker_messages_internal.h"
#include "ipc/ipc_message_macros.h"

#endif  // CHROME_COMMON_WORKER_MESSAGES_H_
