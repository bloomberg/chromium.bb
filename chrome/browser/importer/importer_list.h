// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
#pragma once

#include <string>
#include <vector>

#include "build/build_config.h"
#include "base/basictypes.h"
#include "chrome/browser/importer/importer_data_types.h"

class Importer;

class ImporterList {
 public:
  ImporterList();
  ~ImporterList();

  // Detects the installed browsers and their associated profiles, then
  // stores their information in a list. It returns the list of description
  // of all profiles.
  void DetectSourceProfiles();

  Importer* CreateImporterByType(importer::ProfileType type);

  // Returns the number of different browser profiles you can import from.
  int GetAvailableProfileCount() const;

  // Returns the name of the profile at the 'index' slot. The profiles are
  // ordered such that the profile at index 0 is the likely default browser.
  std::wstring GetSourceProfileNameAt(int index) const;

  // Returns the ProfileInfo at the specified index.  The ProfileInfo should be
  // passed to StartImportSettings().
  const importer::ProfileInfo& GetSourceProfileInfoAt(int index) const;

  // Returns the ProfileInfo with the given browser type.
  const importer::ProfileInfo& GetSourceProfileInfoForBrowserType(
      int browser_type) const;

  // Helper methods for detecting available profiles.
#if defined(OS_WIN)
  void DetectIEProfiles();
#endif
  void DetectFirefoxProfiles();
  void DetectGoogleToolbarProfiles();
#if defined(OS_MACOSX)
  void DetectSafariProfiles();
#endif

 private:
  // The list of profiles with the default one first.
  std::vector<importer::ProfileInfo*> source_profiles_;

  DISALLOW_COPY_AND_ASSIGN(ImporterList);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
