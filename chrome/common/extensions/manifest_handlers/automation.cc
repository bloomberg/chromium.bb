// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/automation.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/manifest_permission.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace automation_errors {
const char kErrorDesktopTrueInteractFalse[] =
    "Cannot specify interactive=false if desktop=true is specified; "
    "interactive=false will be ignored.";
const char kErrorDesktopTrueMatchesSpecified[] =
    "Cannot specify matches for Automation if desktop=true is specified; "
    "matches will be ignored.";
const char kErrorInvalidMatch[] = "Invalid match pattern '*': *";
const char kErrorNoMatchesProvided[] = "No valid match patterns provided.";
}

namespace errors = manifest_errors;
namespace keys = extensions::manifest_keys;
using api::manifest_types::Automation;

class AutomationManifestPermission : public ManifestPermission {
 public:
  explicit AutomationManifestPermission(
      scoped_ptr<const AutomationInfo> automation_info)
      : automation_info_(automation_info.Pass()) {}

  // extensions::ManifestPermission overrides.
  virtual std::string name() const OVERRIDE;

  virtual std::string id() const OVERRIDE;

  virtual bool HasMessages() const OVERRIDE;

  virtual PermissionMessages GetMessages() const OVERRIDE;

  virtual bool FromValue(const base::Value* value) OVERRIDE;

  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE;

  virtual ManifestPermission* Diff(
      const ManifestPermission* rhs) const OVERRIDE;

  virtual ManifestPermission* Union(
      const ManifestPermission* rhs) const OVERRIDE;

  virtual ManifestPermission* Intersect(
      const ManifestPermission* rhs) const OVERRIDE;

 private:
  scoped_ptr<const AutomationInfo> automation_info_;
};

std::string AutomationManifestPermission::name() const {
  return keys::kAutomation;
}

std::string AutomationManifestPermission::id() const {
  return keys::kAutomation;
}

bool AutomationManifestPermission::HasMessages() const {
  return GetMessages().size() > 0;
}

PermissionMessages AutomationManifestPermission::GetMessages() const {
  PermissionMessages messages;
  if (automation_info_->desktop) {
    messages.push_back(PermissionMessage(
        PermissionMessage::kFullAccess,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS)));
  } else if (automation_info_->matches.MatchesAllURLs()) {
    messages.push_back(PermissionMessage(
        PermissionMessage::kHostsAll,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS)));
  } else {
    URLPatternSet regular_hosts;
    std::set<PermissionMessage> message_set;
    ExtensionsClient::Get()->FilterHostPermissions(
        automation_info_->matches, &regular_hosts, &message_set);
    messages.insert(messages.end(), message_set.begin(), message_set.end());

    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    if (!hosts.empty())
      messages.push_back(permission_message_util::CreateFromHostList(hosts));
  }

  return messages;
}

bool AutomationManifestPermission::FromValue(const base::Value* value) {
  base::string16 error;
  automation_info_.reset(AutomationInfo::FromValue(*value,
                                                   NULL /* install_warnings */,
                                                   &error).release());
  return error.empty();
}

scoped_ptr<base::Value> AutomationManifestPermission::ToValue() const {
  return AutomationInfo::ToValue(*automation_info_).Pass();
}

ManifestPermission* AutomationManifestPermission::Diff(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop && !other->automation_info_->desktop;
  bool interact =
      automation_info_->interact && !other->automation_info_->interact;
  URLPatternSet matches;
  URLPatternSet::CreateDifference(
      automation_info_->matches, other->automation_info_->matches, &matches);
  return new AutomationManifestPermission(
      make_scoped_ptr(new const AutomationInfo(desktop, matches, interact)));
}

ManifestPermission* AutomationManifestPermission::Union(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop || other->automation_info_->desktop;
  bool interact =
      automation_info_->interact || other->automation_info_->interact;
  URLPatternSet matches;
  URLPatternSet::CreateUnion(
      automation_info_->matches, other->automation_info_->matches, &matches);
  return new AutomationManifestPermission(
      make_scoped_ptr(new const AutomationInfo(desktop, matches, interact)));
}

ManifestPermission* AutomationManifestPermission::Intersect(
    const ManifestPermission* rhs) const {
  const AutomationManifestPermission* other =
      static_cast<const AutomationManifestPermission*>(rhs);

  bool desktop = automation_info_->desktop && other->automation_info_->desktop;
  bool interact =
      automation_info_->interact && other->automation_info_->interact;
  URLPatternSet matches;
  URLPatternSet::CreateIntersection(
      automation_info_->matches, other->automation_info_->matches, &matches);
  return new AutomationManifestPermission(
      make_scoped_ptr(new const AutomationInfo(desktop, matches, interact)));
}

AutomationHandler::AutomationHandler() {
}

AutomationHandler::~AutomationHandler() {
}

bool AutomationHandler::Parse(Extension* extension, base::string16* error) {
  const base::Value* automation = NULL;
  CHECK(extension->manifest()->Get(keys::kAutomation, &automation));
  std::vector<InstallWarning> install_warnings;
  scoped_ptr<AutomationInfo> info =
      AutomationInfo::FromValue(*automation, &install_warnings, error);
  if (!error->empty())
    return false;

  extension->AddInstallWarnings(install_warnings);

  if (!info)
    return true;

  extension->SetManifestData(keys::kAutomation, info.release());
  return true;
}

const std::vector<std::string> AutomationHandler::Keys() const {
  return SingleKey(keys::kAutomation);
}

ManifestPermission* AutomationHandler::CreatePermission() {
  return new AutomationManifestPermission(
      make_scoped_ptr(new const AutomationInfo));
}

ManifestPermission* AutomationHandler::CreateInitialRequiredPermission(
    const Extension* extension) {
  const AutomationInfo* info = AutomationInfo::Get(extension);
  if (info) {
    return new AutomationManifestPermission(
        make_scoped_ptr(new const AutomationInfo(
            info->desktop, info->matches, info->interact)));
  }
  return NULL;
}

// static
const AutomationInfo* AutomationInfo::Get(const Extension* extension) {
  return static_cast<AutomationInfo*>(
      extension->GetManifestData(keys::kAutomation));
}

// static
scoped_ptr<AutomationInfo> AutomationInfo::FromValue(
    const base::Value& value,
    std::vector<InstallWarning>* install_warnings,
    base::string16* error) {
  scoped_ptr<Automation> automation = Automation::FromValue(value, error);
  if (!automation)
    return scoped_ptr<AutomationInfo>();

  if (automation->as_boolean) {
    if (*automation->as_boolean)
      return make_scoped_ptr(new AutomationInfo());
    return scoped_ptr<AutomationInfo>();
  }
  const Automation::Object& automation_object = *automation->as_object;

  bool desktop = false;
  bool interact = false;
  if (automation_object.desktop && *automation_object.desktop) {
    desktop = true;
    interact = true;
    if (automation_object.interact && !*automation_object.interact) {
      // TODO(aboxhall): Do we want to allow this?
      install_warnings->push_back(
          InstallWarning(automation_errors::kErrorDesktopTrueInteractFalse));
    }
  } else if (automation_object.interact && *automation_object.interact) {
    interact = true;
  }

  URLPatternSet matches;
  bool specified_matches = false;
  if (automation_object.matches) {
    if (desktop) {
      install_warnings->push_back(
          InstallWarning(automation_errors::kErrorDesktopTrueMatchesSpecified));
    } else {
      specified_matches = true;

      for (std::vector<std::string>::iterator it =
               automation_object.matches->begin();
           it != automation_object.matches->end();
           ++it) {
        // TODO(aboxhall): Refactor common logic from content_scripts_handler,
        // manifest_url_handler and user_script.cc into a single location and
        // re-use here.
        URLPattern pattern(URLPattern::SCHEME_ALL &
                           ~URLPattern::SCHEME_CHROMEUI);
        URLPattern::ParseResult parse_result = pattern.Parse(*it);

        if (parse_result != URLPattern::PARSE_SUCCESS) {
          install_warnings->push_back(
              InstallWarning(ErrorUtils::FormatErrorMessage(
                  automation_errors::kErrorInvalidMatch,
                  *it,
                  URLPattern::GetParseResultString(parse_result))));
          continue;
        }

        matches.AddPattern(pattern);
      }
    }
  }
  if (specified_matches && matches.is_empty()) {
    install_warnings->push_back(
        InstallWarning(automation_errors::kErrorNoMatchesProvided));
  }

  return make_scoped_ptr(new AutomationInfo(desktop, matches, interact));
}

// static
scoped_ptr<base::Value> AutomationInfo::ToValue(const AutomationInfo& info) {
  return AsManifestType(info)->ToValue().Pass();
}

// static
scoped_ptr<Automation> AutomationInfo::AsManifestType(
    const AutomationInfo& info) {
  scoped_ptr<Automation> automation(new Automation);
  if (!info.desktop && !info.interact && info.matches.size() == 0) {
    automation->as_boolean.reset(new bool(true));
    return automation.Pass();
  }

  Automation::Object* as_object = new Automation::Object;
  as_object->desktop.reset(new bool(info.desktop));
  as_object->interact.reset(new bool(info.interact));
  if (info.matches.size() > 0) {
    as_object->matches.reset(info.matches.ToStringVector().release());
  }
  automation->as_object.reset(as_object);
  return automation.Pass();
}

AutomationInfo::AutomationInfo() : desktop(false), interact(false) {
}

AutomationInfo::AutomationInfo(bool desktop,
                               const URLPatternSet matches,
                               bool interact)
    : desktop(desktop), matches(matches), interact(interact) {
}

AutomationInfo::~AutomationInfo() {
}

}  // namespace extensions
