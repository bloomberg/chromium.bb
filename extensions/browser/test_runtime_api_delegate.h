// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_
#define EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/api/runtime/runtime_api_delegate.h"

namespace extensions {

class TestRuntimeAPIDelegate : public RuntimeAPIDelegate {
 public:
  TestRuntimeAPIDelegate();
  virtual ~TestRuntimeAPIDelegate();

  // RuntimeAPIDelegate implementation.
  virtual void AddUpdateObserver(UpdateObserver* observer) override;
  virtual void RemoveUpdateObserver(UpdateObserver* observer) override;
  virtual base::Version GetPreviousExtensionVersion(
      const Extension* extension) override;
  virtual void ReloadExtension(const std::string& extension_id) override;
  virtual bool CheckForUpdates(const std::string& extension_id,
                               const UpdateCheckCallback& callback) override;
  virtual void OpenURL(const GURL& uninstall_url) override;
  virtual bool GetPlatformInfo(core_api::runtime::PlatformInfo* info) override;
  virtual bool RestartDevice(std::string* error_message) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestRuntimeAPIDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_TEST_RUNTIME_API_DELEGATE_H_
