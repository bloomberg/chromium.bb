// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_

#include "base/string16.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

class AppEventRouter {
 public:
  // Dispatches the onLaunched event to the given app, providing no launch
  // data.
  static void DispatchOnLaunchedEvent(Profile* profile,
                                      const Extension* extension);
  static void DispatchOnRestartedEvent(Profile* profile,
                                       const Extension* extension);

  // TODO(benwells): Update this comment, it is out of date.
  // Dispatches the onLaunched event to the given app, providing launch data of
  // the form:
  // {
  //   "intent" : {
  //     "type" : "chrome-extension://fileentry",
  //     "data" : a FileEntry,
  //     "postResults" : a null function,
  //     "postFailure" : a null function
  //   }
  // }

  // launchData.intent.data and launchData.intent.postResults are created in a
  // custom dispatch event in javascript. The FileEntry is created from
  // |file_system_id| and |base_name|.
  static void DispatchOnLaunchedEventWithFileEntry(
      Profile* profile,
      const Extension* extension,
      const std::string& handler_id,
      const std::string& mime_type,
      const std::string& file_system_id,
      const std::string& base_name);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_
