// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace extensions {

namespace keys = declarative_content_constants;

namespace {
// Error messages.
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
const char kNoPageAction[] =
    "Can't use declarativeContent.ShowPageAction without a page action";
const char kMissingParameter[] = "Missing parameter is required: %s";

#define INPUT_FORMAT_VALIDATE(test) do { \
    if (!(test)) { \
      *bad_message = true; \
      return scoped_refptr<ContentAction>(NULL); \
    } \
  } while (0)

//
// The following are concrete actions.
//

// Action that instructs to show an extension's page action.
class ShowPageAction : public ContentAction {
 public:
  ShowPageAction() {}

  static scoped_refptr<ContentAction> Create(const Extension* extension,
                                             const base::DictionaryValue* dict,
                                             std::string* error,
                                             bool* bad_message) {
    // We can't show a page action if the extension doesn't have one.
    if (ActionInfo::GetPageActionInfo(extension) == NULL) {
      *error = kNoPageAction;
      return scoped_refptr<ContentAction>();
    }
    return scoped_refptr<ContentAction>(new ShowPageAction);
  }

  // Implementation of ContentAction:
  virtual Type GetType() const OVERRIDE { return ACTION_SHOW_PAGE_ACTION; }
  virtual void Apply(const std::string& extension_id,
                     const base::Time& extension_install_time,
                     ApplyInfo* apply_info) const OVERRIDE {
    ExtensionAction* action = GetPageAction(apply_info->profile, extension_id);
    action->DeclarativeShow(ExtensionTabUtil::GetTabId(apply_info->tab));
    ExtensionActionAPI::Get(apply_info->profile)->NotifyChange(
        action, apply_info->tab, apply_info->profile);
  }
  // The page action is already showing, so nothing needs to be done here.
  virtual void Reapply(const std::string& extension_id,
                       const base::Time& extension_install_time,
                       ApplyInfo* apply_info) const OVERRIDE {}
  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const OVERRIDE {
    if (ExtensionAction* action =
            GetPageAction(apply_info->profile, extension_id)) {
      action->UndoDeclarativeShow(ExtensionTabUtil::GetTabId(apply_info->tab));
      ExtensionActionAPI::Get(apply_info->profile)->NotifyChange(
          action, apply_info->tab, apply_info->profile);
    }
  }

 private:
  static ExtensionAction* GetPageAction(Profile* profile,
                                        const std::string& extension_id) {
    const Extension* extension =
        ExtensionRegistry::Get(profile)
            ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
    if (!extension)
      return NULL;
    return ExtensionActionManager::Get(profile)->GetPageAction(*extension);
  }
  virtual ~ShowPageAction() {}

  DISALLOW_COPY_AND_ASSIGN(ShowPageAction);
};

// Action that injects a content script.
class RequestContentScript : public ContentAction {
 public:
  RequestContentScript(const std::vector<std::string>& css_file_names,
                       const std::vector<std::string>& js_file_names,
                       bool all_frames,
                       bool match_about_blank)
      : css_file_names_(css_file_names),
        js_file_names_(js_file_names),
        all_frames_(all_frames),
        match_about_blank_(match_about_blank) {}

  static scoped_refptr<ContentAction> Create(const Extension* extension,
                                             const base::DictionaryValue* dict,
                                             std::string* error,
                                             bool* bad_message);

  // Implementation of ContentAction:
  virtual Type GetType() const OVERRIDE {
    return ACTION_REQUEST_CONTENT_SCRIPT;
  }

  virtual void Apply(const std::string& extension_id,
                     const base::Time& extension_install_time,
                     ApplyInfo* apply_info) const OVERRIDE {
    // TODO(markdittmer): Invoke UserScriptMaster declarative script loader:
    // load new user script.
  }

  virtual void Reapply(const std::string& extension_id,
                       const base::Time& extension_install_time,
                       ApplyInfo* apply_info) const OVERRIDE {
    // TODO(markdittmer): Invoke UserScriptMaster declarative script loader:
    // load new user script.
  }

  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const OVERRIDE {
    // TODO(markdittmer): Invoke UserScriptMaster declarative script loader:
    // do not load user script if Apply() runs again on the same page.
  }

 private:
  virtual ~RequestContentScript() {}

  std::vector<std::string> css_file_names_;
  std::vector<std::string> js_file_names_;
  bool all_frames_;
  bool match_about_blank_;

  DISALLOW_COPY_AND_ASSIGN(RequestContentScript);
};

// Helper for getting JS collections into C++.
static bool AppendJSStringsToCPPStrings(const base::ListValue& append_strings,
                                        std::vector<std::string>* append_to) {
  for (base::ListValue::const_iterator it = append_strings.begin();
       it != append_strings.end();
       ++it) {
    std::string value;
    if ((*it)->GetAsString(&value)) {
      append_to->push_back(value);
    } else {
      return false;
    }
  }

  return true;
}

// static
scoped_refptr<ContentAction> RequestContentScript::Create(
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  std::vector<std::string> css_file_names;
  std::vector<std::string> js_file_names;
  bool all_frames = false;
  bool match_about_blank = false;
  const base::ListValue* list_value;

  if (!dict->HasKey(keys::kCss) && !dict->HasKey(keys::kJs)) {
    *error = base::StringPrintf(kMissingParameter, "css or js");
    return scoped_refptr<ContentAction>();
  }
  if (dict->HasKey(keys::kCss)) {
    INPUT_FORMAT_VALIDATE(dict->GetList(keys::kCss, &list_value));
    INPUT_FORMAT_VALIDATE(
        AppendJSStringsToCPPStrings(*list_value, &css_file_names));
  }
  if (dict->HasKey(keys::kJs)) {
    INPUT_FORMAT_VALIDATE(dict->GetList(keys::kJs, &list_value));
    INPUT_FORMAT_VALIDATE(
        AppendJSStringsToCPPStrings(*list_value, &js_file_names));
  }
  if (dict->HasKey(keys::kAllFrames)) {
    INPUT_FORMAT_VALIDATE(dict->GetBoolean(keys::kAllFrames, &all_frames));
  }
  if (dict->HasKey(keys::kMatchAboutBlank)) {
    INPUT_FORMAT_VALIDATE(
        dict->GetBoolean(keys::kMatchAboutBlank, &match_about_blank));
  }

  return scoped_refptr<ContentAction>(new RequestContentScript(
      css_file_names, js_file_names, all_frames, match_about_blank));
}

struct ContentActionFactory {
  // Factory methods for ContentAction instances. |extension| is the extension
  // for which the action is being created. |dict| contains the json dictionary
  // that describes the action. |error| is used to return error messages in case
  // the extension passed an action that was syntactically correct but
  // semantically incorrect. |bad_message| is set to true in case |dict| does
  // not confirm to the validated JSON specification.
  typedef scoped_refptr<ContentAction>(*FactoryMethod)(
      const Extension* /* extension */,
      const base::DictionaryValue* /* dict */,
      std::string* /* error */,
      bool* /* bad_message */);
  // Maps the name of a declarativeContent action type to the factory
  // function creating it.
  std::map<std::string, FactoryMethod> factory_methods;

  ContentActionFactory() {
    factory_methods[keys::kShowPageAction] =
        &ShowPageAction::Create;
    factory_methods[keys::kRequestContentScript] =
        &RequestContentScript::Create;
  }
};

base::LazyInstance<ContentActionFactory>::Leaky
    g_content_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// ContentAction
//

ContentAction::ContentAction() {}

ContentAction::~ContentAction() {}

// static
scoped_refptr<ContentAction> ContentAction::Create(
    const Extension* extension,
    const base::Value& json_action,
    std::string* error,
    bool* bad_message) {
  *error = "";
  *bad_message = false;

  const base::DictionaryValue* action_dict = NULL;
  INPUT_FORMAT_VALIDATE(json_action.GetAsDictionary(&action_dict));

  std::string instance_type;
  INPUT_FORMAT_VALIDATE(
      action_dict->GetString(keys::kInstanceType, &instance_type));

  ContentActionFactory& factory = g_content_action_factory.Get();
  std::map<std::string, ContentActionFactory::FactoryMethod>::iterator
      factory_method_iter = factory.factory_methods.find(instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(
        extension, action_dict, error, bad_message);

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return scoped_refptr<ContentAction>();
}

}  // namespace extensions
