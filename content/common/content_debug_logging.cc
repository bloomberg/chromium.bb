// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/content_debug_logging.h"

#include "base/logging.h"

namespace content {
namespace debug {

namespace {

// These should be initialized by the process as early in main() as
// possible, so should be thread-safe.
RecordMsgFn* g_record_handler = NULL;
GetMessagesFn* g_get_handler = NULL;

}  // namespace

void RegisterMessageHandlers(RecordMsgFn* record_handler,
                             GetMessagesFn* get_handler) {
  g_record_handler = record_handler;
  g_get_handler = get_handler;
}

void RecordMsg(int bug_id, const std::string& msg) {
  // TODO(shess): It might be useful to log warnings or even DCHECK if
  // there is no handler, since that implies that the messages are not
  // being recorded.  Unfortunately, it would also cause dirty test
  // output.
  if (g_record_handler)
    (*g_record_handler)(bug_id, msg);
}

bool GetMessages(int bug_id, std::vector<std::string>* msgs) {
  if (g_get_handler)
    return (*g_get_handler)(bug_id, msgs);

  msgs->clear();
  return false;
}

}  // namespace debug
}  // namespace content
