// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_view_host_chromeos.h"

#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"

using content::WebContents;

namespace chromeos {

BalloonViewHost::BalloonViewHost(Balloon* balloon)
    : ::BalloonViewHost(balloon) {
}

BalloonViewHost::~BalloonViewHost() {
}

bool BalloonViewHost::AddWebUIMessageCallback(
    const std::string& message,
    const MessageCallback& callback) {
  std::pair<MessageCallbackMap::iterator, bool> ret =
      message_callbacks_.insert(std::make_pair(message, callback));
  return ret.second;
}

bool BalloonViewHost::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // No context menu for chromeos system notifications.
  return true;
}

void BalloonViewHost::WebUISend(WebContents* tab,
                                const GURL& source_url,
                                const std::string& name,
                                const ListValue& args) {
  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(name);
  if (callback == message_callbacks_.end())
    return;

  // Run callback.
  callback->second.Run(&args);
}

}  // namespace chromeos
