// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/guest_view_event.h"

#include "extensions/browser/event_router.h"
#include "extensions/browser/guest_view/guest_view_base.h"

namespace extensions {

GuestViewEvent::GuestViewEvent(const std::string& name,
                               scoped_ptr<base::DictionaryValue> args)
    : name_(name),
      args_(args.Pass()) {
}

GuestViewEvent::~GuestViewEvent() {
}

void GuestViewEvent::Dispatch(GuestViewBase* guest, int instance_id) {
  EventFilteringInfo info;
  info.SetInstanceID(instance_id);
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(args_.release());

  EventRouter::DispatchEvent(
      guest->owner_web_contents(),
      guest->browser_context(),
      guest->owner_extension_id(),
      name_,
      args.Pass(),
      EventRouter::USER_GESTURE_UNKNOWN,
      info);

  delete this;
}

}  // namespace extensions
