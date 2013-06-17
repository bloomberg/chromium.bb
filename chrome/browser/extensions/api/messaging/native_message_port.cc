// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/messaging/native_message_port.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

NativeMessagePort::NativeMessagePort(NativeMessageProcessHost* native_process)
    : native_process_(native_process) {
}

NativeMessagePort::~NativeMessagePort() {
  content::BrowserThread::DeleteSoon(
      content::BrowserThread::IO, FROM_HERE, native_process_);
}

void NativeMessagePort::DispatchOnMessage(scoped_ptr<base::ListValue> message,
                                          int target_port_id) {
  std::string message_as_json;
  if (!message->empty()) {
    DCHECK_EQ(1u, message->GetSize());
    base::Value* value = NULL;
    message->Get(0, &value);
    base::JSONWriter::Write(value, &message_as_json);
  }
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&NativeMessageProcessHost::Send,
                 base::Unretained(native_process_),
                 message_as_json));
}

}  // namespace extensions
