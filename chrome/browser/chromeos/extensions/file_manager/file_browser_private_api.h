// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_

#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace file_manager {

class EventRouter;

// Manages and registers the fileBrowserPrivate API with the extension system.
class FileBrowserPrivateAPI : public KeyedService {
 public:
  explicit FileBrowserPrivateAPI(Profile* profile);
  virtual ~FileBrowserPrivateAPI();

  // KeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Convenience function to return the FileBrowserPrivateAPI for a Profile.
  static FileBrowserPrivateAPI* Get(Profile* profile);

  EventRouter* event_router() { return event_router_.get(); }

 private:
  scoped_ptr<EventRouter> event_router_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_H_
