// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COURGETTE_COURGETTE_CONFIG_H_
#define COURGETTE_COURGETTE_CONFIG_H_

#include "base/macros.h"
#include "base/memory/singleton.h"

namespace base {
class CommandLine;
}  // namespace base

namespace courgette {

// A singleton class to hold global configurations for Courgette, mainly for
// experimental purposes.
class CourgetteConfig {
 public:
  static CourgetteConfig* GetInstance();

  void Initialize(const base::CommandLine& command_line);

  bool is_experimental() const { return is_experimental_; }

  uint32 ensemble_version() const;

 private:
  bool is_experimental_;

  CourgetteConfig();

  friend struct DefaultSingletonTraits<CourgetteConfig>;
};

}  // namespace courgette

#endif  // COURGETTE_COURGETTE_CONFIG_H_
