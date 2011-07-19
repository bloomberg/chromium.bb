// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_AUTOMATION_DOM_AUTOMATION_V8_EXTENSION_H_
#define CHROME_RENDERER_AUTOMATION_DOM_AUTOMATION_V8_EXTENSION_H_
#pragma once

#include "v8/include/v8.h"

class DomAutomationV8Extension {
 public:
  static const char* kName;
  static v8::Extension* Get();
};

#endif  // CHROME_RENDERER_AUTOMATION_DOM_AUTOMATION_V8_EXTENSION_H_
