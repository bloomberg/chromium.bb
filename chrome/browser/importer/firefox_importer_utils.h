// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"
#include "build/build_config.h"

class GURL;
class TemplateURL;

namespace base {
class DictionaryValue;
class FilePath;
}

#if defined(OS_WIN)
// Detects which version of Firefox is installed from registry. Returns its
// major version, and drops the minor version. Returns 0 if failed. If there are
// indicators of both Firefox 2 and Firefox 3 it is biased to return the biggest
// version.
int GetCurrentFirefoxMajorVersionFromRegistry();

// Detects where Firefox lives. Returns an empty path if Firefox is not
// installed.
base::FilePath GetFirefoxInstallPathFromRegistry();
#endif  // OS_WIN

#if defined(OS_MACOSX)
// Get the directory in which the Firefox .dylibs live, we need to load these
// in order to decoded FF profile passwords.
// The Path is usuall FF App Bundle/Contents/Mac OS/
// Returns empty path on failure.
base::FilePath GetFirefoxDylibPath();
#endif  // OS_MACOSX

// Returns the path to the Firefox profile.
base::FilePath GetFirefoxProfilePath();

// Detects version of Firefox and installation path for the given Firefox
// profile.
bool GetFirefoxVersionAndPathFromProfile(const base::FilePath& profile_path,
                                         int* version,
                                         base::FilePath* app_path);

// Gets the full path of the profiles.ini file. This file records the profiles
// that can be used by Firefox. Returns an empty path if failed.
base::FilePath GetProfilesINI();

// Parses the profile.ini file, and stores its information in |root|.
// This file is a plain-text file. Key/value pairs are stored one per line, and
// they are separated in different sections. For example:
//   [General]
//   StartWithLastProfile=1
//
//   [Profile0]
//   Name=default
//   IsRelative=1
//   Path=Profiles/abcdefeg.default
// We set "[value]" in path "<Section>.<Key>". For example, the path
// "Genenral.StartWithLastProfile" has the value "1".
void ParseProfileINI(const base::FilePath& file, base::DictionaryValue* root);

// Returns true if we want to add the URL to the history. We filter out the URL
// with a unsupported scheme.
bool CanImportURL(const GURL& url);

// Parses the OpenSearch XML files in |xml_files| and populates |search_engines|
// with the resulting TemplateURLs.
void ParseSearchEnginesFromXMLFiles(const std::vector<base::FilePath>& xml_files,
                                    std::vector<TemplateURL*>* search_engines);

// Returns the home page set in Firefox in a particular profile.
GURL GetHomepage(const base::FilePath& profile_path);

// Checks to see if this home page is a default home page, as specified by
// the resource file browserconfig.properties in the Firefox application
// directory.
bool IsDefaultHomepage(const GURL& homepage, const base::FilePath& app_path);

// Parses the prefs found in the file |pref_file| and puts the key/value pairs
// in |prefs|. Keys are strings, and values can be strings, booleans or
// integers.  Returns true if it succeeded, false otherwise (in which case
// |prefs| is not filled).
// Note: for strings, only valid UTF-8 string values are supported. If a
// key/pair is not valid UTF-8, it is ignored and will not appear in |prefs|.
bool ParsePrefFile(const base::FilePath& pref_file, base::DictionaryValue* prefs);

// Parses the value of a particular firefox preference from a string that is the
// contents of the prefs file.
std::string GetPrefsJsValue(const std::string& prefs,
                            const std::string& pref_key);

// Returns the localized Firefox branding name.
// This is useful to differentiate between Firefox and Iceweasel.
// If anything goes wrong while trying to obtain the branding name,
// the function assumes it's Firefox.
string16 GetFirefoxImporterName(const base::FilePath& app_path);

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX_IMPORTER_UTILS_H_
