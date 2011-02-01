// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_IO_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_IO_EVENT_ROUTER_H_
#pragma once

#include <string>

#include "base/ref_counted.h"

class Profile;

// For now, this just forwards events from the IO thread to the
// ExtensionEventRouter on the UI thread.
// TODO(mpcomplete): eventually I want this to have its own copy of the event
// listeners so it can bypass the jump to the UI thread.
class ExtensionIOEventRouter
    : public base::RefCountedThreadSafe<ExtensionIOEventRouter> {
 public:
  explicit ExtensionIOEventRouter(Profile* profile);
  ~ExtensionIOEventRouter();

  void DestroyingProfile() { profile_ = NULL; }

  // Dispatch the named event to every extension listening to that event.
  void DispatchEvent(const std::string& event_name,
                     const std::string& event_args) const;

 private:
  Profile* profile_;

  void DispatchEventOnUIThread(const std::string& event_name,
                               const std::string& event_args) const;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIOEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_IO_EVENT_ROUTER_H_
