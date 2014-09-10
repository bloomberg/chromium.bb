// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_H_

#include "base/memory/scoped_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace file_manager {

class EventRouter;

// Manages and registers the fileManagerPrivate API with the extension system.
class FileManagerPrivateAPI : public KeyedService {
 public:
  explicit FileManagerPrivateAPI(Profile* profile);
  virtual ~FileManagerPrivateAPI();

  // KeyedService overrides.
  virtual void Shutdown() OVERRIDE;

  // Convenience function to return the FileManagerPrivateAPI for a Profile.
  static FileManagerPrivateAPI* Get(Profile* profile);

  EventRouter* event_router() { return event_router_.get(); }

 private:
  scoped_ptr<EventRouter> event_router_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_H_
