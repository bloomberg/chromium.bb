// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/declarative_user_script_manager.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/declarative/declarative_constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace extensions {

namespace {
// Error messages.
const char kInvalidIconDictionary[] =
    "Icon dictionary must be of the form {\"19\": ImageData1, \"38\": "
    "ImageData2}";
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
const char kMissingInstanceTypeError[] = "Action is missing instanceType";
const char kMissingParameter[] = "Missing parameter is required: %s";
const char kNoPageAction[] =
    "Can't use declarativeContent.ShowPageAction without a page action";
const char kNoPageOrBrowserAction[] =
    "Can't use declarativeContent.SetIcon without a page or browser action";

//
// The following are concrete actions.
//

// Action that instructs to show an extension's page action.
class ShowPageAction : public ContentAction {
 public:
  ShowPageAction() {}
  ~ShowPageAction() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::DictionaryValue* dict,
      std::string* error) {
    // We can't show a page action if the extension doesn't have one.
    if (ActionInfo::GetPageActionInfo(extension) == NULL) {
      *error = kNoPageAction;
      return std::unique_ptr<ContentAction>();
    }
    return base::WrapUnique(new ShowPageAction);
  }

  // Implementation of ContentAction:
  void Apply(const ApplyInfo& apply_info) const override {
    ExtensionAction* action =
        GetPageAction(apply_info.browser_context, apply_info.extension);
    action->DeclarativeShow(ExtensionTabUtil::GetTabId(apply_info.tab));
    ExtensionActionAPI::Get(apply_info.browser_context)->NotifyChange(
        action, apply_info.tab, apply_info.browser_context);
  }
  // The page action is already showing, so nothing needs to be done here.
  void Reapply(const ApplyInfo& apply_info) const override {}
  void Revert(const ApplyInfo& apply_info) const override {
    if (ExtensionAction* action =
            GetPageAction(apply_info.browser_context, apply_info.extension)) {
      action->UndoDeclarativeShow(ExtensionTabUtil::GetTabId(apply_info.tab));
      ExtensionActionAPI::Get(apply_info.browser_context)->NotifyChange(
          action, apply_info.tab, apply_info.browser_context);
    }
  }

 private:
  static ExtensionAction* GetPageAction(
      content::BrowserContext* browser_context,
      const Extension* extension) {
    return ExtensionActionManager::Get(browser_context)
        ->GetPageAction(*extension);
  }

  DISALLOW_COPY_AND_ASSIGN(ShowPageAction);
};

// Action that sets an extension's action icon.
class SetIcon : public ContentAction {
 public:
  SetIcon(const gfx::Image& icon, ActionInfo::Type action_type)
      : icon_(icon), action_type_(action_type) {}
  ~SetIcon() override {}

  static std::unique_ptr<ContentAction> Create(
      content::BrowserContext* browser_context,
      const Extension* extension,
      const base::DictionaryValue* dict,
      std::string* error);

  // Implementation of ContentAction:
  void Apply(const ApplyInfo& apply_info) const override {
    Profile* profile = Profile::FromBrowserContext(apply_info.browser_context);
    ExtensionAction* action = GetExtensionAction(profile,
                                                 apply_info.extension);
    if (action) {
      action->DeclarativeSetIcon(ExtensionTabUtil::GetTabId(apply_info.tab),
                                 apply_info.priority,
                                 icon_);
      ExtensionActionAPI::Get(profile)
          ->NotifyChange(action, apply_info.tab, profile);
    }
  }

  void Reapply(const ApplyInfo& apply_info) const override {}

  void Revert(const ApplyInfo& apply_info) const override {
    Profile* profile = Profile::FromBrowserContext(apply_info.browser_context);
    ExtensionAction* action = GetExtensionAction(profile,
                                                 apply_info.extension);
    if (action) {
      action->UndoDeclarativeSetIcon(
          ExtensionTabUtil::GetTabId(apply_info.tab),
          apply_info.priority,
          icon_);
      ExtensionActionAPI::Get(apply_info.browser_context)
          ->NotifyChange(action, apply_info.tab, profile);
    }
  }

 private:
  ExtensionAction* GetExtensionAction(Profile* profile,
                                      const Extension* extension) const {
    switch (action_type_) {
      case ActionInfo::TYPE_BROWSER:
        return ExtensionActionManager::Get(profile)
            ->GetBrowserAction(*extension);
      case ActionInfo::TYPE_PAGE:
        return ExtensionActionManager::Get(profile)->GetPageAction(*extension);
      default:
        NOTREACHED();
    }
    return NULL;
  }

  gfx::Image icon_;
  ActionInfo::Type action_type_;

  DISALLOW_COPY_AND_ASSIGN(SetIcon);
};

// Helper for getting JS collections into C++.
static bool AppendJSStringsToCPPStrings(const base::ListValue& append_strings,
                                        std::vector<std::string>* append_to) {
  for (auto it = append_strings.begin(); it != append_strings.end(); ++it) {
    std::string value;
    if (it->GetAsString(&value)) {
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
  // that describes the action. |error| is used to return error messages.
  using FactoryMethod = std::unique_ptr<ContentAction> (*)(
      content::BrowserContext* /* browser_context */,
      const Extension* /* extension */,
      const base::DictionaryValue* /* dict */,
      std::string* /* error */);
  // Maps the name of a declarativeContent action type to the factory
  // function creating it.
  std::map<std::string, FactoryMethod> factory_methods;

  ContentActionFactory() {
    factory_methods[declarative_content_constants::kShowPageAction] =
        &ShowPageAction::Create;
    factory_methods[declarative_content_constants::kRequestContentScript] =
        &RequestContentScript::Create;
    factory_methods[declarative_content_constants::kSetIcon] = &SetIcon::Create;
  }
};

base::LazyInstance<ContentActionFactory>::Leaky
    g_content_action_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

//
// RequestContentScript
//

struct RequestContentScript::ScriptData {
  ScriptData();
  ~ScriptData();

  std::vector<std::string> css_file_names;
  std::vector<std::string> js_file_names;
  bool all_frames;
  bool match_about_blank;
};

RequestContentScript::ScriptData::ScriptData()
    : all_frames(false),
      match_about_blank(false) {}
RequestContentScript::ScriptData::~ScriptData() {}

// static
std::unique_ptr<ContentAction> RequestContentScript::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error) {
  ScriptData script_data;
  if (!InitScriptData(dict, error, &script_data))
    return std::unique_ptr<ContentAction>();

  return base::WrapUnique(
      new RequestContentScript(browser_context, extension, script_data));
}

// static
std::unique_ptr<ContentAction> RequestContentScript::CreateForTest(
    DeclarativeUserScriptMaster* master,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error) {
  // Simulate ContentAction-level initialization. Check that instance type is
  // RequestContentScript.
  error->clear();
  const base::DictionaryValue* action_dict = NULL;
  std::string instance_type;
  if (!(json_action.GetAsDictionary(&action_dict) &&
        action_dict->GetString(declarative_content_constants::kInstanceType,
                               &instance_type) &&
        instance_type ==
            std::string(declarative_content_constants::kRequestContentScript)))
    return std::unique_ptr<ContentAction>();

  // Normal RequestContentScript data initialization.
  ScriptData script_data;
  if (!InitScriptData(action_dict, error, &script_data))
    return std::unique_ptr<ContentAction>();

  // Inject provided DeclarativeUserScriptMaster, rather than looking it up
  // using a BrowserContext.
  return base::WrapUnique(
      new RequestContentScript(master, extension, script_data));
}

// static
bool RequestContentScript::InitScriptData(const base::DictionaryValue* dict,
                                          std::string* error,
                                          ScriptData* script_data) {
  const base::ListValue* list_value = NULL;

  if (!dict->HasKey(declarative_content_constants::kCss) &&
      !dict->HasKey(declarative_content_constants::kJs)) {
    *error = base::StringPrintf(kMissingParameter, "css or js");
    return false;
  }
  if (dict->HasKey(declarative_content_constants::kCss)) {
    if (!dict->GetList(declarative_content_constants::kCss, &list_value) ||
        !AppendJSStringsToCPPStrings(*list_value,
                                     &script_data->css_file_names)) {
      return false;
    }
  }
  if (dict->HasKey(declarative_content_constants::kJs)) {
    if (!dict->GetList(declarative_content_constants::kJs, &list_value) ||
        !AppendJSStringsToCPPStrings(*list_value,
                                     &script_data->js_file_names)) {
      return false;
    }
  }
  if (dict->HasKey(declarative_content_constants::kAllFrames)) {
    if (!dict->GetBoolean(declarative_content_constants::kAllFrames,
                          &script_data->all_frames))
      return false;
  }
  if (dict->HasKey(declarative_content_constants::kMatchAboutBlank)) {
    if (!dict->GetBoolean(declarative_content_constants::kMatchAboutBlank,
                          &script_data->match_about_blank)) {
      return false;
    }
  }

  return true;
}

RequestContentScript::RequestContentScript(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const ScriptData& script_data) {
  HostID host_id(HostID::EXTENSIONS, extension->id());
  InitScript(host_id, extension, script_data);

  master_ = DeclarativeUserScriptManager::Get(browser_context)
                ->GetDeclarativeUserScriptMasterByID(host_id);
  AddScript();
}

RequestContentScript::RequestContentScript(
    DeclarativeUserScriptMaster* master,
    const Extension* extension,
    const ScriptData& script_data) {
  HostID host_id(HostID::EXTENSIONS, extension->id());
  InitScript(host_id, extension, script_data);

  master_ = master;
  AddScript();
}

RequestContentScript::~RequestContentScript() {
  DCHECK(master_);
  master_->RemoveScript(UserScriptIDPair(script_.id(), script_.host_id()));
}

void RequestContentScript::InitScript(const HostID& host_id,
                                      const Extension* extension,
                                      const ScriptData& script_data) {
  script_.set_id(UserScript::GenerateUserScriptID());
  script_.set_host_id(host_id);
  script_.set_run_location(UserScript::BROWSER_DRIVEN);
  script_.set_match_all_frames(script_data.all_frames);
  script_.set_match_about_blank(script_data.match_about_blank);
  for (auto it = script_data.css_file_names.cbegin();
       it != script_data.css_file_names.cend(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.css_scripts().push_back(std::make_unique<UserScript::File>(
        resource.extension_root(), resource.relative_path(), url));
  }
  for (auto it = script_data.js_file_names.cbegin();
       it != script_data.js_file_names.cend(); ++it) {
    GURL url = extension->GetResourceURL(*it);
    ExtensionResource resource = extension->GetResource(*it);
    script_.js_scripts().push_back(std::make_unique<UserScript::File>(
        resource.extension_root(), resource.relative_path(), url));
  }
}

void RequestContentScript::AddScript() {
  DCHECK(master_);
  master_->AddScript(UserScript::CopyMetadataFrom(script_));
}

void RequestContentScript::Apply(const ApplyInfo& apply_info) const {
  InstructRenderProcessToInject(apply_info.tab, apply_info.extension);
}

void RequestContentScript::Reapply(const ApplyInfo& apply_info) const {
  InstructRenderProcessToInject(apply_info.tab, apply_info.extension);
}

void RequestContentScript::Revert(const ApplyInfo& apply_info) const {}

void RequestContentScript::InstructRenderProcessToInject(
    content::WebContents* contents,
    const Extension* extension) const {
  content::RenderFrameHost* render_frame_host = contents->GetMainFrame();
  render_frame_host->Send(new ExtensionMsg_ExecuteDeclarativeScript(
      render_frame_host->GetRoutingID(),
      SessionTabHelper::IdForTab(contents).id(), extension->id(), script_.id(),
      contents->GetLastCommittedURL()));
}

// static
std::unique_ptr<ContentAction> SetIcon::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::DictionaryValue* dict,
    std::string* error) {
  // We can't set a page or action's icon if the extension doesn't have one.
  ActionInfo::Type type;
  if (ActionInfo::GetPageActionInfo(extension) != NULL) {
    type = ActionInfo::TYPE_PAGE;
  } else if (ActionInfo::GetBrowserActionInfo(extension) != NULL) {
    type = ActionInfo::TYPE_BROWSER;
  } else {
    *error = kNoPageOrBrowserAction;
    return std::unique_ptr<ContentAction>();
  }

  gfx::ImageSkia icon;
  const base::DictionaryValue* canvas_set = NULL;
  if (dict->GetDictionary("imageData", &canvas_set) &&
      !ExtensionAction::ParseIconFromCanvasDictionary(*canvas_set, &icon)) {
    *error = kInvalidIconDictionary;
    return std::unique_ptr<ContentAction>();
  }
  return base::WrapUnique(new SetIcon(gfx::Image(icon), type));
}

//
// ContentAction
//

ContentAction::~ContentAction() {}

// static
std::unique_ptr<ContentAction> ContentAction::Create(
    content::BrowserContext* browser_context,
    const Extension* extension,
    const base::Value& json_action,
    std::string* error) {
  error->clear();
  const base::DictionaryValue* action_dict = NULL;
  std::string instance_type;
  if (!(json_action.GetAsDictionary(&action_dict) &&
        action_dict->GetString(declarative_content_constants::kInstanceType,
                               &instance_type))) {
    *error = kMissingInstanceTypeError;
    return std::unique_ptr<ContentAction>();
  }

  ContentActionFactory& factory = g_content_action_factory.Get();
  auto factory_method_iter = factory.factory_methods.find(instance_type);
  if (factory_method_iter != factory.factory_methods.end())
    return (*factory_method_iter->second)(
        browser_context, extension, action_dict, error);

  *error = base::StringPrintf(kInvalidInstanceTypeError, instance_type.c_str());
  return std::unique_ptr<ContentAction>();
}

ContentAction::ContentAction() {}

}  // namespace extensions
