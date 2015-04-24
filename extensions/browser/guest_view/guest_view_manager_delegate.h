// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_DELEGATE_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_DELEGATE_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace extensions {
class GuestViewBase;
}  // namespace extensions

namespace guestview {

// A GuestViewManagerDelegate interface allows GuestViewManager to delegate
// responsibilities to other modules in Chromium. Different builds of Chromium
// may use different GuestViewManagerDelegate implementations. For example,
// mobile builds of Chromium do not include an extensions module and so
// permission checks would be different, and IsOwnedByExtension would always
// return false.
class GuestViewManagerDelegate {
 public:
  virtual ~GuestViewManagerDelegate() {}

  // Dispatches the event with |name| with the provided |args| to the embedder
  // of the given |guest| with |instance_id| for routing.
  virtual void DispatchEvent(const std::string& event_name,
                             scoped_ptr<base::DictionaryValue> args,
                             extensions::GuestViewBase* guest,
                             int instance_id) = 0;

  // Indicates whether the |guest| can be used within the context of where it
  // was created.
  virtual bool IsGuestAvailableToContext(extensions::GuestViewBase* guest) = 0;

  // Indicates whether the |guest| is owned by an extension or Chrome App.
  virtual bool IsOwnedByExtension(extensions::GuestViewBase* guest) = 0;

  // Registers additional GuestView types the delegator (GuestViewManger) can
  // create.
  virtual void RegisterAdditionalGuestViewTypes() = 0;
};

}  // namespace guestview

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_GUEST_VIEW_MANAGER_DELEGATE_H_
