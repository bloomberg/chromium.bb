// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_

#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "build/build_config.h"

class DictionaryValue;
class GURL;
class TemplateURL;

#if defined(OS_WIN)
// Detects which version of Firefox is installed from registry. Returns its
// major version, and drops the minor version. Returns 0 if
// failed. If there are indicators of both FF2 and FF3 it is
// biased to return the biggest version.
int GetCurrentFirefoxMajorVersionFromRegistry();

// Detects where Firefox lives.  Returns a empty string if Firefox
// is not installed.
std::wstring GetFirefoxInstallPathFromRegistry();
#endif

// Detects version of Firefox and installation path from given Firefox profile
bool GetFirefoxVersionAndPathFromProfile(const std::wstring& profile_path,
                                         int* version,
                                         std::wstring* app_path);

// Gets the full path of the profiles.ini file. This file records
// the profiles that can be used by Firefox.  Returns an empty
// string if failed.
FilePath GetProfilesINI();

// Parses the profile.ini file, and stores its information in |root|.
// This file is a plain-text file. Key/value pairs are stored one per
// line, and they are separeated in different sections. For example:
//   [General]
//   StartWithLastProfile=1
//
//   [Profile0]
//   Name=default
//   IsRelative=1
//   Path=Profiles/abcdefeg.default
// We set "[value]" in path "<Section>.<Key>". For example, the path
// "Genenral.StartWithLastProfile" has the value "1".
void ParseProfileINI(std::wstring file, DictionaryValue* root);

// Returns true if we want to add the URL to the history. We filter
// out the URL with a unsupported scheme.
bool CanImportURL(const GURL& url);

// Parses the OpenSearch XML files in |xml_files| and populates |search_engines|
// with the resulting TemplateURLs.
void ParseSearchEnginesFromXMLFiles(const std::vector<std::wstring>& xml_files,
                                    std::vector<TemplateURL*>* search_engines);

// Returns the index of the default search engine in the |search_engines| list.
// If none is found, -1 is returned.
int GetFirefoxDefaultSearchEngineIndex(
    const std::vector<TemplateURL*>& search_engines,
    const std::wstring& profile_path);

// Returns the home page set in Firefox in a particular profile.
GURL GetHomepage(const std::wstring& profile_path);

// Checks to see if this home page is a default home page, as specified by
// the resource file browserconfig.properties in the Firefox application
// directory.
bool IsDefaultHomepage(const GURL& homepage, const std::wstring& app_path);


#endif  // CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
