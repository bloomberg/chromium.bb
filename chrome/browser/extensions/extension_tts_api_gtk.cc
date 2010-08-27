// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extension_tts_api.h"

bool ExtensionTtsSpeakFunction::RunImpl() {
  return false;
}

bool ExtensionTtsStopSpeakingFunction::RunImpl() {
  return false;
}

bool ExtensionTtsIsSpeakingFunction::RunImpl() {
  return false;
}
