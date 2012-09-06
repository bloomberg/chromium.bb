// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONTENT_DEBUG_LOGGING_H_
#define CONTENT_PUBLIC_BROWSER_CONTENT_DEBUG_LOGGING_H_

#include "content/common/content_export.h"

#include <string>
#include <vector>

// When debugging single processes, a common technique is to store
// breakpad keys which can be analyzed from crashes.  That technique
// does not work well for multi-process bugs.  This code can
// be used to implement a ring buffer in the browser which stores the
// most-recent N (browser-defined) messages relating to a bug.
//
// The intent is that users will call RecordMsg() to record info for
// the bug, and GetMessages() to fetch the recorded info.  In the
// browser process, these will be backed by a local data structure.
// In other processes, they will be backed by messages forwarding to
// the browser process.

// TODO(shess): Since bug notes will be distributed across the code,
// it would be inconvenient to require some other include to have
// kBugNNNNN constants for users to use.  Those could be in this file,
// but for now they're a DCHECK in chrome_delegate_main.cc (which
// implements the actual storage).

namespace content {
namespace debug {

// RecordMsg() and GetMessages() forward to the handlers set by
// RegisterMessageHandlers().  If no handlers are set (or NULL
// handlers are set), the messages are simply dropped.
//
// If GetMessages() returns false, there was no handler (or the
// handler was unable to get messages).  *msgs will be cleared.  If
// GetMessages() returns true, zero or more of the most recent
// messages previously sent to RecordMsg() will be in *msgs, in the
// order recorded, mod things like forwarded delay between processes.
CONTENT_EXPORT void RecordMsg(int bug_id, const std::string& msg);
CONTENT_EXPORT bool GetMessages(int bug_id, std::vector<std::string>* msgs);

// Used to register handlers which will differ per process.
typedef void RecordMsgFn(int bug_id, const std::string& msg);
typedef bool GetMessagesFn(int bug_id, std::vector<std::string>* msgs);
CONTENT_EXPORT void RegisterMessageHandlers(RecordMsgFn* record_handler,
                                            GetMessagesFn* get_handler);

// TODO(shess): It would be convenient to expose functions to do things like:
// - load values into breakpad.
// - load values into breakpad and dump without crashing.
// - load values into breakpad and crash.
// - log values to the console.
// But this is mostly Chromium-level stuff.

}  // namespace debug
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONTENT_DEBUG_LOGGING_H_
