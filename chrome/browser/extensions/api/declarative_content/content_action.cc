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
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"

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
      return false; \
    } \
  } while (0)

//
// The following are concrete actions.
//

// Action that instructs to show an extension's page action.
class ShowPageAction : public ContentAction {
 public:
  ShowPageAction() {}

  static scoped_refptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
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

struct ContentActionFactory {
  // Factory methods for ContentAction instances. |extension| is the extension
  // for which the action is being created. |dict| contains the json dictionary
  // that describes the action. |error| is used to return error messages in case
  // the extension passed an action that was syntactically correct but
  // semantically incorrect. |bad_message| is set to true in case |dict| does
  // not confirm to the validated JSON specification.
  typedef scoped_refptr<ContentAction>(*FactoryMethod)(
      content::BrowserContext* /* browser_context */,
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
// RequestContentScript
//

RequestContentScript::ScriptData::ScriptData() {}
RequestContentScript::ScriptData::~ScriptData() {}

// static
scoped_refptr<ContentAction> RequestContentScript::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error,
    bool* bad_message) {
  ScriptData script_data;
  if (!InitScriptData(dict, error, bad_message, &script_data))
    return scoped_refptr<ContentAction>();

  return scoped_refptr<ContentAction>(new RequestContentScript(
      browser_context,
      extension,
      script_data));
}

// static
scoped_refptr<ContentAction> RequestContentScript::CreateForTest(
    DeclarativeUserScriptMaster* master,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error,
    bool* bad_message) {
  // Simulate ContentAction-level initialization. Check that instance type is
  // RequestContentScript.
  ContentAction::ResetErrorData(error, bad_message);
  const base::DictionaryValue* action_dict = NULL;
  std::string instance_type;
  if (!ContentAction::Validate(
          json_action,
          error,
          bad_message,
          &action_dict,
          &instance_type) ||
      instance_type != std::string(keys::kRequestContentScript))
    return scoped_refptr<ContentAction>();

  // Normal RequestContentScript data initialization.
  ScriptData script_data;
  if (!InitScriptData(action_dict, error, bad_message, &script_data))
    return scoped_refptr<ContentAction>();

  // Inject provided DeclarativeUserScriptMaster, rather than looking it up
  // using a BrowserContext.
  return scoped_refptr<ContentAction>(new RequestContentScript(
      master,
      extension,
      script_data));
}

// static
bool RequestContentScript::InitScriptData(const base::DictionaryValue* dict,
                                          std::string* error,
                                          bool* bad_message,
                                          ScriptData* script_data) {
  const base::ListValue* list_value = NULL;

  if (!dict->HasKey(keys::kCss) && !dict->HasKey(keys::kJs)) {
    *error = base::StringPrintf(kMissingParameter, "css or js");
    return false;
  }
  if (dict->HasKey(keys::kCss)) {
    INPUT_FORMAT_VALIDATE(dict->GetList(keys::kCss, &list_value));
    INPUT_FORMAT_VALIDATE(
        AppendJSStringsToCPPStrings(*list_value, &script_data->css_file_names));
  }
  if (dict->HasKey(keys::kJs)) {
    INPUT_FORMAT_VALIDATE(dict->GetList(keys::kJs, &list_value));
    INPUT_FORMAT_VALIDATE(
        AppendJSStringsToCPPStrings(*list_value, &script_data->js_file_names));
  }
  if (dict->HasKey(keys::kAllFrames)) {
    INPUT_FORMAT_VALIDATE(
        dict->GetBoolean(keys::kAllFrames, &script_data->all_frames));
  }
  if (dict->HasKey(keys::kMatchAboutBlank)) {
    INPUT_FORMAT_VALIDATE(
        dict->GetBoolean(
            keys::kMatchAboutBlank,
            &script_data->match_about_blank));
  }

  return true;
}

RequestContentScript::RequestContentScript(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const ScriptData& script_data) {
  InitScript(extension, script_data);

  master_ =
      ExtensionSystem::Get(browser_context)->
      GetDeclarativeUserScriptMasterByExtension(extension->id());
  AddScript();
}

RequestContentScript::RequestContentScript(
    DeclarativeUserScriptMaster* master,
    const Extension* extension,
    const ScriptData& script_data) {
  InitScript(extension, script_data);

  master_ = master;
  AddScript();
}

RequestContentScript::~RequestContentScript() {
  DCHECK(master_);
  master_->RemoveScript(script_);
}

void RequestContentScript::InitScript(const Extension* extension,
                                      const ScriptData& script_data) {
  script_.set_id(UserScript::GenerateUserScriptID());
  script_.set_extension_id(extension->id());
  script_.set_run_location(UserScript::BROWSER_DRIVEN);
  script_.set_match_all_frames(script_data.all_frames);
  script_.set_match_about_blank(script_data.match_about_blank);
  for (std::vector<std::string>::const_iterator it =
           script_data.css_file_names.begin();
       it != script_data.css_file_names.end(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.css_scripts().push_back(UserScript::File(
        resource.extension_root(), resource.relative_path(), url));
  }
  for (std::vector<std::string>::const_iterator it =
           script_data.js_file_names.begin();
       it != script_data.js_file_names.end(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.js_scripts().push_back(UserScript::File(
        resource.extension_root(), resource.relative_path(), url));
  }
}

ContentAction::Type RequestContentScript::GetType() const {
  return ACTION_REQUEST_CONTENT_SCRIPT;
}

void RequestContentScript::Apply(const std::string& extension_id,
                                 const base::Time& extension_install_time,
                                 ApplyInfo* apply_info) const {
  InstructRenderProcessToInject(apply_info->tab, extension_id);
}

void RequestContentScript::Reapply(const std::string& extension_id,
                                   const base::Time& extension_install_time,
                                   ApplyInfo* apply_info) const {
  InstructRenderProcessToInject(apply_info->tab, extension_id);
}

void RequestContentScript::Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const {}

void RequestContentScript::InstructRenderProcessToInject(
    content::WebContents* contents,
    const std::string& extension_id) const {
  content::RenderViewHost* render_view_host = contents->GetRenderViewHost();
  render_view_host->Send(new ExtensionMsg_ExecuteDeclarativeScript(
      render_view_host->GetRoutingID(),
      SessionTabHelper::IdForTab(contents),
      extension_id,
      script_.id(),
      contents->GetLastCommittedURL()));
}

//
// ContentAction
//

ContentAction::ContentAction() {}

ContentAction::~ContentAction() {}

// static
scoped_refptr<ContentAction> ContentAction::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error,
    bool* bad_message) {
  ResetErrorData(error, bad_message);
  const base::DictionaryValue* action_dict = NULL;
  std::string instance_type;
  if (!Validate(json_action, error, bad_message, &action_dict, &instance_type))
    return scoped_refptr<ContentAction>();

  ContentActionFactory& factory = g_content_action_factory.Get();
  std::map<std::string, ContentActionFactory::FactoryMethod>::iterator
      factory_method_iter = factory.factory_methods.find(instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(
        browser_context, extension, action_dict, error, bad_message);

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return scoped_refptr<ContentAction>();
}

bool ContentAction::Validate(const base::Value& json_action,
                             std::string* error,
                             bool* bad_message,
                             const base::DictionaryValue** action_dict,
                             std::string* instance_type) {
  INPUT_FORMAT_VALIDATE(json_action.GetAsDictionary(action_dict));
  INPUT_FORMAT_VALIDATE(
      (*action_dict)->GetString(keys::kInstanceType, instance_type));
  return true;
}

}  // namespace extensions
