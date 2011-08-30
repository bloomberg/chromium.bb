// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/automation/dom_automation_v8_extension.h"

#include "chrome/renderer/extensions/extension_base.h"
#include "grit/renderer_resources.h"

const char* DomAutomationV8Extension::kName = "chrome/domautomation";

v8::Extension* DomAutomationV8Extension::Get() {
  static v8::Extension* extension =
      new v8::Extension(
          kName, ExtensionBase::GetStringResource(IDR_DOM_AUTOMATION_JS), 0,
          NULL);
  return extension;
}
