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
class WebIntentsDispatcher;
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

  // Dispatches the onLaunched event to the given app, providing launch data of
  // the form:
  // {
  //   "intent" : {
  //     "action" : |action|,
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
      const string16& action,
      const std::string& handler_id,
      const std::string& mime_type,
      const std::string& file_system_id,
      const std::string& base_name);

  // Dispatches the onLaunched event to the app implemented by |extension|
  // running in |profile|. The event parameter launchData will have a field
  // called intent, populated by |web_intent_data|.
  static void DispatchOnLaunchedEventWithWebIntent(
      Profile* profile,
      const Extension* extension,
      content::WebIntentsDispatcher* intents_dispatcher,
      content::WebContents* source);
};

class AppRuntimePostIntentResponseFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.runtime.postIntentResponse",
                             APP_RUNTIME_POSTINTENTRESPONSE)

 protected:
  virtual ~AppRuntimePostIntentResponseFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_RUNTIME_APP_RUNTIME_API_H_
