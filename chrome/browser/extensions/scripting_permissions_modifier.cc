// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scripting_permissions_modifier.h"

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

URLPatternSet FilterImpliedAllHostsPatterns(const URLPatternSet& patterns) {
  URLPatternSet result;
  for (const URLPattern& pattern : patterns) {
    if (pattern.ImpliesAllHosts())
      result.AddPattern(pattern);
  }
  return result;
}

}  // namespace

ScriptingPermissionsModifier::ScriptingPermissionsModifier(
    content::BrowserContext* browser_context,
    const scoped_refptr<const Extension>& extension)
    : browser_context_(browser_context), extension_(extension) {}

ScriptingPermissionsModifier::~ScriptingPermissionsModifier() {}

bool ScriptingPermissionsModifier::CanAffectExtension(
    const PermissionSet& permissions) const {
  // An extension may require user action to execute scripts iff the extension
  // shows up in chrome:extensions (so the user can grant withheld permissions),
  // is not part of chrome or corporate policy, not on the scripting whitelist,
  // and requires enough permissions that we should withhold them.
  return extension_->ShouldDisplayInExtensionSettings() &&
         !Manifest::IsPolicyLocation(extension_->location()) &&
         !Manifest::IsComponentLocation(extension_->location()) &&
         !PermissionsData::CanExecuteScriptEverywhere(extension_.get()) &&
         permissions.ShouldWarnAllHosts();
}

bool ScriptingPermissionsModifier::HasAffectedExtension() const {
  return extension_->permissions_data()->HasWithheldImpliedAllHosts() ||
         util::HasSetAllowedScriptingOnAllUrls(extension_->id(),
                                               browser_context_);
}

void ScriptingPermissionsModifier::GrantHostPermission(const GURL& url) const {
  GURL origin = url.GetOrigin();
  URLPatternSet new_explicit_hosts;
  URLPatternSet new_scriptable_hosts;

  const PermissionSet& withheld_permissions =
      extension_->permissions_data()->withheld_permissions();
  if (withheld_permissions.explicit_hosts().MatchesURL(url)) {
    new_explicit_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), origin);
  }
  if (withheld_permissions.scriptable_hosts().MatchesURL(url)) {
    new_scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(),
                                   origin);
  }

  PermissionsUpdater(browser_context_)
      .AddPermissions(extension_.get(),
                      PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                                    new_explicit_hosts, new_scriptable_hosts));
}

bool ScriptingPermissionsModifier::HasGrantedHostPermission(
    const GURL& url) const {
  GURL origin = url.GetOrigin();
  const PermissionSet& required_permissions =
      PermissionsParser::GetRequiredPermissions(extension_.get());
  if (!extension_->permissions_data()
           ->active_permissions()
           .effective_hosts()
           .MatchesURL(origin))
    return false;
  scoped_ptr<const PermissionSet> granted_permissions;
  scoped_ptr<const PermissionSet> withheld_permissions;
  WithholdPermissions(required_permissions, &granted_permissions,
                      &withheld_permissions, true);
  if (!granted_permissions->effective_hosts().MatchesURL(origin) &&
      withheld_permissions->effective_hosts().MatchesURL(origin))
    return true;

  return false;
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) const {
  DCHECK(HasGrantedHostPermission(url));

  GURL origin = url.GetOrigin();
  URLPatternSet explicit_hosts;
  URLPatternSet scriptable_hosts;
  const PermissionSet& active_permissions =
      extension_->permissions_data()->active_permissions();
  if (active_permissions.explicit_hosts().MatchesURL(url))
    explicit_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), origin);
  if (active_permissions.scriptable_hosts().MatchesURL(url))
    scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), origin);

  PermissionsUpdater(browser_context_)
      .RemovePermissions(
          extension_.get(),
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        explicit_hosts, scriptable_hosts),
          PermissionsUpdater::REMOVE_HARD);
}

void ScriptingPermissionsModifier::WithholdPermissions(
    const PermissionSet& permissions,
    scoped_ptr<const PermissionSet>* granted_permissions_out,
    scoped_ptr<const PermissionSet>* withheld_permissions_out,
    bool use_initial_state) const {
  bool should_withhold = false;
  if (CanAffectExtension(permissions)) {
    if (use_initial_state) {
      // If the user ever set the extension's "all-urls" preference, then the
      // initial state was withheld. This is important, since the all-urls
      // permission should be shown as revokable. Otherwise, default to whatever
      // the system setting is.
      should_withhold =
          util::HasSetAllowedScriptingOnAllUrls(extension_->id(),
                                                browser_context_) ||
          !util::DefaultAllowedScriptingOnAllUrls();
    } else {
      should_withhold =
          !util::AllowedScriptingOnAllUrls(extension_->id(), browser_context_);
    }
  }

  if (!should_withhold) {
    *granted_permissions_out = permissions.Clone();
    withheld_permissions_out->reset(new PermissionSet());
    return;
  }

  auto segregate_url_permissions = [](const URLPatternSet& patterns,
                                      URLPatternSet* granted,
                                      URLPatternSet* withheld) {
    for (const URLPattern& pattern : patterns) {
      if (pattern.ImpliesAllHosts())
        withheld->AddPattern(pattern);
      else
        granted->AddPattern(pattern);
    }
  };

  URLPatternSet granted_explicit_hosts;
  URLPatternSet withheld_explicit_hosts;
  URLPatternSet granted_scriptable_hosts;
  URLPatternSet withheld_scriptable_hosts;
  segregate_url_permissions(permissions.explicit_hosts(),
                            &granted_explicit_hosts, &withheld_explicit_hosts);
  segregate_url_permissions(permissions.scriptable_hosts(),
                            &granted_scriptable_hosts,
                            &withheld_scriptable_hosts);

  granted_permissions_out->reset(
      new PermissionSet(permissions.apis(), permissions.manifest_permissions(),
                        granted_explicit_hosts, granted_scriptable_hosts));
  withheld_permissions_out->reset(
      new PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        withheld_explicit_hosts, withheld_scriptable_hosts));
}

void ScriptingPermissionsModifier::GrantWithheldImpliedAllHosts() const {
  const PermissionSet& withheld =
      extension_->permissions_data()->withheld_permissions();

  PermissionSet permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      FilterImpliedAllHostsPatterns(withheld.explicit_hosts()),
      FilterImpliedAllHostsPatterns(withheld.scriptable_hosts()));
  PermissionsUpdater(browser_context_)
      .AddPermissions(extension_.get(), permissions);
}

void ScriptingPermissionsModifier::WithholdImpliedAllHosts() const {
  const PermissionSet& active =
      extension_->permissions_data()->active_permissions();
  PermissionSet permissions(
      APIPermissionSet(), ManifestPermissionSet(),
      FilterImpliedAllHostsPatterns(active.explicit_hosts()),
      FilterImpliedAllHostsPatterns(active.scriptable_hosts()));
  PermissionsUpdater(browser_context_)
      .RemovePermissions(extension_.get(), permissions,
                         PermissionsUpdater::REMOVE_HARD);
}

}  // namespace extensions
