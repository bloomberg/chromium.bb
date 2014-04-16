// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/request_manager.h"

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {
namespace file_system_provider {

// Provided file system implementation. Forwards requests between providers and
// clients.
class ProvidedFileSystem : public ProvidedFileSystemInterface {
 public:
  ProvidedFileSystem(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info);
  virtual ~ProvidedFileSystem();

  // ProvidedFileSystemInterface overrides.
  virtual bool RequestUnmount(
      const fileapi::AsyncFileUtil::StatusCallback& callback) OVERRIDE;
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const OVERRIDE;
  virtual RequestManager* GetRequestManager() OVERRIDE;

 private:
  extensions::EventRouter* event_router_;
  RequestManager request_manager_;
  ProvidedFileSystemInfo file_system_info_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedFileSystem);
};

}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_H_
