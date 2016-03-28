// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_
#define BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_

namespace blimp {
namespace engine {

struct EngineSettings {
  // -------WebPreferences-------
  bool record_whole_document;

  EngineSettings();
  EngineSettings(const EngineSettings& other);
  ~EngineSettings();
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_ENGINE_SETTINGS_H_
