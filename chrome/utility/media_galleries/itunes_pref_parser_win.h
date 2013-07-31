// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_ITUNES_PREF_PARSER_WIN_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_ITUNES_PREF_PARSER_WIN_H_

#include <string>

#include "base/files/file_path.h"

namespace itunes {

// Extracts the library XML location from the iTunes preferences XML data.
// Return the path the the library XML location if found. The minimal
// valid snippet of XML is:
//
//   <plist>
//     <dict>
//       <key>User Preferences</key>
//       <dict>
//         <key>iTunes Library XML Location:1</key>
//         <data>Base64 encoded w string path</data>
//       </dict>
//     </dict>
//   </plist>
//
base::FilePath::StringType FindLibraryLocationInPrefXml(
    const std::string& pref_xml_data);

}  // namespace itunes

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_ITUNES_PREF_PARSER_WIN_H_
