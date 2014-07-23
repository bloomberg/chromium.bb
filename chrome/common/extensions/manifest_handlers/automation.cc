// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/automation.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/api/manifest_types.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"

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
  if (specified_matches && matches.is_empty())
    install_warnings->push_back(
        InstallWarning(automation_errors::kErrorNoMatchesProvided));

  return make_scoped_ptr(
      new AutomationInfo(desktop, matches, interact, specified_matches));
}

AutomationInfo::AutomationInfo()
    : desktop(false), interact(false), specified_matches(false) {
}
AutomationInfo::AutomationInfo(bool desktop,
                               const URLPatternSet& matches,
                               bool interact,
                               bool specified_matches)
    : desktop(desktop),
      matches(matches),
      interact(interact),
      specified_matches(specified_matches) {
}

AutomationInfo::~AutomationInfo() {
}

}  // namespace extensions
