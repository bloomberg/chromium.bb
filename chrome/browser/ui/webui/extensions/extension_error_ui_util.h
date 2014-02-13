// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_UI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_UI_UTIL_H_

#include "base/callback.h"

class Profile;

namespace base {
class DictionaryValue;
}

namespace extensions {
namespace error_ui_util {

typedef base::Callback<void(const base::DictionaryValue&)>
    RequestFileSourceCallback;

// Read an extension's file which caused an error. |args| specifies the file to
// be read and extra details about the file, |profile| is the active profile to
// use, and |response| is called upon completed.
void HandleRequestFileSource(const base::DictionaryValue* args,
                             Profile* profile,
                             const RequestFileSourceCallback& response);

// Open the Developer Tools to inspect an error caused by an extension. |args|
// specify the context in which the error occurred.
void HandleOpenDevTools(const base::DictionaryValue* args);

}  // namespace error_ui_util
}  // namespace extensions

#endif  // CHROME_BROWSER_UI_WEBUI_EXTENSIONS_EXTENSION_ERROR_UI_UTIL_H_
