// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
#define CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
#pragma once

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/platform_file.h"
#include "chrome/browser/nacl_host/nacl_validation_cache.h"

// Represents shared state for all NaClProcessHost objects in the browser.
class NaClBrowser {
 public:
  static NaClBrowser* GetInstance();

  bool IrtAvailable() const;

  base::PlatformFile IrtFile() const;

  // Asynchronously attempt to get the IRT open.
  bool EnsureIrtAvailable();

  // Make sure the IRT gets opened and follow up with the reply when it's ready.
  bool MakeIrtAvailable(const base::Closure& reply);

  // Path to IRT. Available even before IRT is loaded.
  const FilePath& GetIrtFilePath();

  NaClValidationCache validation_cache;

 private:
  friend struct DefaultSingletonTraits<NaClBrowser>;

  NaClBrowser();
  ~NaClBrowser();

  void InitIrtFilePath();

  void OpenIrtLibraryFile();

  static void DoOpenIrtLibraryFile() {
    GetInstance()->OpenIrtLibraryFile();
  }

  base::PlatformFile irt_platform_file_;

  FilePath irt_filepath_;

  DISALLOW_COPY_AND_ASSIGN(NaClBrowser);
};

#endif  // CHROME_BROWSER_NACL_HOST_NACL_BROWSER_H_
