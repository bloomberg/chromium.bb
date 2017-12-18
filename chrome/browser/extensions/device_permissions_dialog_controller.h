// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chooser_controller/chooser_controller.h"
#include "extensions/browser/api/device_permissions_prompt.h"

class DevicePermissionsDialogController
    : public ChooserController,
      public extensions::DevicePermissionsPrompt::Prompt::Observer {
 public:
  DevicePermissionsDialogController(
      content::RenderFrameHost* owner,
      scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt);
  ~DevicePermissionsDialogController() override;

  // ChooserController:
  bool ShouldShowHelpButton() const override;
  bool AllowMultipleSelection() const override;
  base::string16 GetNoOptionsText() const override;
  base::string16 GetOkButtonLabel() const override;
  size_t NumOptions() const override;
  base::string16 GetOption(size_t index) const override;
  void Select(const std::vector<size_t>& indices) override;
  void Cancel() override;
  void Close() override;
  void OpenHelpCenterUrl() const override;

  // extensions::DevicePermissionsPrompt::Prompt::Observer:
  void OnDeviceAdded(size_t index, const base::string16& device_name) override;
  void OnDeviceRemoved(size_t index,
                       const base::string16& device_name) override;

 private:
  scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt_;
  // Maps from device name to number of devices.
  std::unordered_map<base::string16, int> device_name_map_;

  DISALLOW_COPY_AND_ASSIGN(DevicePermissionsDialogController);
};

#endif  // CHROME_BROWSER_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLLER_H_
