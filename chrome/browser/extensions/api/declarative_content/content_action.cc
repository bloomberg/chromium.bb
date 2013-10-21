// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include <map>

#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/declarative_content/content_constants.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents.h"

namespace extensions {

namespace keys = declarative_content_constants;

namespace {
// Error messages.
const char kInvalidInstanceTypeError[] =
    "An action has an invalid instanceType: %s";
const char kNoPageAction[] =
    "Can't use declarativeContent.ShowPageAction without a page action";

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
    GetPageAction(apply_info->profile, extension_id)->DeclarativeShow(
        ExtensionTabUtil::GetTabId(apply_info->tab));
    apply_info->tab->NotifyNavigationStateChanged(
        content::INVALIDATE_TYPE_PAGE_ACTIONS);
  }
  virtual void Revert(const std::string& extension_id,
                      const base::Time& extension_install_time,
                      ApplyInfo* apply_info) const OVERRIDE {
    if (ExtensionAction* action =
            GetPageAction(apply_info->profile, extension_id)) {
      action->UndoDeclarativeShow(ExtensionTabUtil::GetTabId(apply_info->tab));
      apply_info->tab->NotifyNavigationStateChanged(
          content::INVALIDATE_TYPE_PAGE_ACTIONS);
    }
  }

 private:
  static ExtensionAction* GetPageAction(Profile* profile,
                                        const std::string& extension_id) {
    ExtensionService* service =
        ExtensionSystem::Get(profile)->extension_service();
    const Extension* extension = service->GetInstalledExtension(extension_id);
    if (!extension)
      return NULL;
    return ExtensionActionManager::Get(profile)->GetPageAction(*extension);
  }
  virtual ~ShowPageAction() {}

  DISALLOW_COPY_AND_ASSIGN(ShowPageAction);
};

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
