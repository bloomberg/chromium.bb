// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_

#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

namespace app_file_handler_util {
struct GrantedFileEntry;
}

class AppEventRouter {
 public:
  // Dispatches the onLaunched event to the given app.
  static void DispatchOnLaunchedEvent(Profile* profile,
                                      const Extension* extension);

  // Dispatches the onRestarted event to the given app, providing a list of
  // restored file entries from the previous run.
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

  // The FileEntry is created from |file_system_id| and |base_name|.
  // |handler_id| corresponds to the id of the file_handlers item in the
  // manifest that resulted in a match which triggered this launch.
  static void DispatchOnLaunchedEventWithFileEntry(
      Profile* profile,
      const Extension* extension,
      const std::string& handler_id,
      const std::string& mime_type,
      const extensions::app_file_handler_util::GrantedFileEntry& file_entry);

  // |handler_id| corresponds to the id of the url_handlers item
  // in the manifest that resulted in a match which triggered this launch.
  static void DispatchOnLaunchedEventWithUrl(
      Profile* profile,
      const Extension* extension,
      const std::string& handler_id,
      const GURL& url,
      const GURL& referrer_url);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_
