// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
#define CHROME_BROWSER_EXTENSIONS_API_TABS_EXECUTE_CODE_IN_TAB_FUNCTION_H__

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/user_script.h"

namespace extensions {
namespace api {
namespace tabs {
struct InjectDetails;
}  // namespace tabs
}  // namespace api
}  // namespace extensions

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public AsyncExtensionFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  virtual ~ExecuteCodeInTabFunction();

  // ExtensionFunction:
  virtual bool HasPermission() OVERRIDE;
  virtual bool RunImpl() OVERRIDE;

  // Message handler.
  virtual void OnExecuteCodeFinished(const std::string& error,
                                     int32 on_page_id,
                                     const GURL& on_url,
                                     const ListValue& script_result);

 private:
  // Initialize the |execute_tab_id_| and |details_| if they haven't already
  // been. Returns whether initialization was successful.
  bool Init();

  // Called when contents from the file whose path is specified in JSON
  // arguments has been loaded.
  void DidLoadFile(bool success, const std::string& data);

  // Runs on FILE thread. Loads message bundles for the extension and
  // localizes the CSS data. Calls back DidLoadAndLocalizeFile on the UI thread.
  void LocalizeCSS(
      const std::string& data,
      const std::string& extension_id,
      const FilePath& extension_path,
      const std::string& extension_default_locale);

  // Called when contents from the loaded file have been localized.
  void DidLoadAndLocalizeFile(bool success, const std::string& data);

  // Run in UI thread.  Code string contains the code to be executed. Returns
  // true on success. If true is returned, this does an AddRef.
  bool Execute(const std::string& code_string);

  // Id of tab which executes code.
  int execute_tab_id_;

  // The injection details.
  scoped_ptr<extensions::api::tabs::InjectDetails> details_;

  // Contains extension resource built from path of file which is
  // specified in JSON arguments.
  ExtensionResource resource_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 private:
  virtual ~TabsExecuteScriptFunction() {}

  virtual void OnExecuteCodeFinished(const std::string& error,
                                     int32 on_page_id,
                                     const GURL& on_url,
                                     const ListValue& script_result) OVERRIDE;

  DECLARE_EXTENSION_FUNCTION_NAME("tabs.executeScript")
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  virtual ~TabsInsertCSSFunction() {}

  DECLARE_EXTENSION_FUNCTION_NAME("tabs.insertCSS")
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_EXECUTE_CODE_IN_TAB_FUNCTION_H__
