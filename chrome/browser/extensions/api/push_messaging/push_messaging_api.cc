// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/push_messaging/push_messaging_api.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/experimental_push_messaging.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"

namespace extensions {

namespace glue = extensions::api::experimental_push_messaging;

PushMessagingEventRouter::PushMessagingEventRouter(Profile* profile)
    : profile_(profile) {
}

PushMessagingEventRouter::~PushMessagingEventRouter() {}

void PushMessagingEventRouter::Init() {
  // TODO(dcheng): Add hooks into InvalidationHandler when landed.
}

void PushMessagingEventRouter::OnMessage(const std::string& extension_id,
                                         int subchannel,
                                         const std::string& payload) {
  glue::Message message;
  message.subchannel_id = subchannel;
  message.payload = payload;

  scoped_ptr<base::ListValue> args(glue::OnMessage::Create(message));
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id,
      event_names::kOnPushMessage,
      args.Pass(),
      profile_,
      GURL());
}

}  // namespace extensions
