// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/declarative_user_script_master.h"
#include "extensions/browser/api/declarative/declarative_rule.h"

namespace base {
class Time;
class Value;
}

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {
class Extension;

// Base class for all ContentActions of the declarative content API.
class ContentAction : public base::RefCounted<ContentAction> {
 public:
  // Type identifiers for concrete ContentActions.
  enum Type {
    ACTION_SHOW_PAGE_ACTION,
    ACTION_REQUEST_CONTENT_SCRIPT,
    ACTION_SET_ICON,
  };

  struct ApplyInfo {
    content::BrowserContext* browser_context;
    content::WebContents* tab;
    int priority;
  };

  ContentAction();

  virtual Type GetType() const = 0;

  // Applies or reverts this ContentAction on a particular tab for a particular
  // extension.  Revert exists to keep the actions up to date as the page
  // changes.  Reapply exists to reapply changes to a new page, even if the
  // previous page also matched relevant conditions.
  virtual void Apply(const std::string& extension_id,
                     const base::Time& extension_install_time,
                     ApplyInfo* apply_info) const = 0;
  virtual void Reapply(const std::string& extension_id,
                       const base::Time& extension_install_time,
                       ApplyInfo* apply_info) const = 0;
  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const = 0;

  // Factory method that instantiates a concrete ContentAction
  // implementation according to |json_action|, the representation of the
  // ContentAction as received from the extension API.
  // Sets |error| and returns NULL in case of a semantic error that cannot
  // be caught by schema validation. Sets |bad_message| and returns NULL
  // in case the input is syntactically unexpected.
  static scoped_refptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::Value& json_action,
      std::string* error,
      bool* bad_message);

  // Shared procedure for resetting error state within factories.
  static void ResetErrorData(std::string* error, bool* bad_message) {
    *error = "";
    *bad_message = false;
  }

  // Shared procedure for validating JSON data.
  static bool Validate(const base::Value& json_action,
                       std::string* error,
                       bool* bad_message,
                       const base::DictionaryValue** action_dict,
                       std::string* instance_type);

 protected:
  friend class base::RefCounted<ContentAction>;
  virtual ~ContentAction();
};

// Action that injects a content script.
class RequestContentScript : public ContentAction {
 public:
  struct ScriptData;

  RequestContentScript(content::BrowserContext* browser_context,
                       const Extension* extension,
                       const ScriptData& script_data);
  RequestContentScript(DeclarativeUserScriptMaster* master,
                       const Extension* extension,
                       const ScriptData& script_data);

  static scoped_refptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::DictionaryValue* dict,
      std::string* error,
      bool* bad_message);

  static scoped_refptr<ContentAction> CreateForTest(
      DeclarativeUserScriptMaster* master,
      const Extension* extension,
      const base::Value& json_action,
      std::string* error,
      bool* bad_message);

  static bool InitScriptData(const base::DictionaryValue* dict,
                             std::string* error,
                             bool* bad_message,
                             ScriptData* script_data);

  // Implementation of ContentAction:
  virtual Type GetType() const OVERRIDE;

  virtual void Apply(const std::string& extension_id,
                     const base::Time& extension_install_time,
                     ApplyInfo* apply_info) const OVERRIDE;

  virtual void Reapply(const std::string& extension_id,
                       const base::Time& extension_install_time,
                       ApplyInfo* apply_info) const OVERRIDE;

  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const OVERRIDE;

 private:
  void InitScript(const Extension* extension, const ScriptData& script_data);

  void AddScript() {
    DCHECK(master_);
    master_->AddScript(script_);
  }

  virtual ~RequestContentScript();

  void InstructRenderProcessToInject(content::WebContents* contents,
                                     const std::string& extension_id) const;

  UserScript script_;
  DeclarativeUserScriptMaster* master_;

  DISALLOW_COPY_AND_ASSIGN(RequestContentScript);
};

typedef DeclarativeActionSet<ContentAction> ContentActionSet;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_ACTION_H_
