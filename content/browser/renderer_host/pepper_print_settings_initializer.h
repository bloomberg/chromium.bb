// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PRINT_SETTINGS_INITIALIZER_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PRINT_SETTINGS_INITIALIZER_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "ppapi/c/dev/pp_print_settings_dev.h"

namespace content {

// This is a helper class for |PepperMessageFilter| which caches the default
// print settings which are obtained from the system upon startup.
// TODO(raymes): Ideally we would grab the default print settings every time
// they are requested (because there is a chance they could change) however we
// need to get the print settings on the UI thread and sync messages can't be
// processed on the UI thread on Windows. We want this message to be synchronous
// because it is called by Flash. There might be a better solution than this.
class PepperPrintSettingsInitializer
    : public base::RefCountedThreadSafe<PepperPrintSettingsInitializer> {
 public:
  PepperPrintSettingsInitializer();
  void Initialize();
  // Returns true if default settings are available and places them in
  // |settings|.
  bool GetDefaultPrintSettings(PP_PrintSettings_Dev* settings) const;

 private:
  friend class base::RefCountedThreadSafe<PepperPrintSettingsInitializer>;

  virtual ~PepperPrintSettingsInitializer();

  void DoInitialize();
  void SetDefaultPrintSettings(const PP_PrintSettings_Dev& settings);

  bool initialized_;
  PP_PrintSettings_Dev default_print_settings_;

  DISALLOW_COPY_AND_ASSIGN(PepperPrintSettingsInitializer);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PRINT_SETTINGS_INITIALIZER_H_
