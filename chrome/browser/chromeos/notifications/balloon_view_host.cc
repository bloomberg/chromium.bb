// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/notifications/balloon_view_host.h"

#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"

namespace chromeos {

BalloonViewHost::~BalloonViewHost() {
  STLDeleteContainerPairSecondPointers(message_callbacks_.begin(),
                                       message_callbacks_.end());
}

bool BalloonViewHost::AddDOMUIMessageCallback(
    const std::string& message,
    MessageCallback* callback) {
  std::pair<MessageCallbackMap::iterator, bool> ret;
  ret = message_callbacks_.insert(std::make_pair(message, callback));
  if (!ret.second)
    delete callback;
  return ret.second;
}

void BalloonViewHost::ProcessWebUIMessage(
    const ViewHostMsg_DomMessage_Params& params) {
  ::BalloonViewHost::ProcessWebUIMessage(params);

  // Look up the callback for this message.
  MessageCallbackMap::const_iterator callback =
      message_callbacks_.find(params.name);
  if (callback == message_callbacks_.end())
    return;

  // Run callback.
  callback->second->Run(&params.arguments);
}

}  // namespace chromeos
