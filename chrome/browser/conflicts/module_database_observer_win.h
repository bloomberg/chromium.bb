// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONFLICTS_MODULE_DATABASE_OBSERVER_WIN_H_
#define CHROME_BROWSER_CONFLICTS_MODULE_DATABASE_OBSERVER_WIN_H_

struct ModuleInfoKey;
struct ModuleInfoData;

class ModuleDatabaseObserver {
 public:
  virtual ~ModuleDatabaseObserver() = default;

  // Invoked when a new module is loaded into Chrome, but after it has been
  // inspected on disk.
  virtual void OnNewModuleFound(const ModuleInfoKey& module_key,
                                const ModuleInfoData& module_data) = 0;
};

#endif  // CHROME_BROWSER_CONFLICTS_MODULE_DATABASE_OBSERVER_WIN_H_
