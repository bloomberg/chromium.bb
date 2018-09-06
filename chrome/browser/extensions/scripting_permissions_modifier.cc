// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/scripting_permissions_modifier.h"

#include "base/feature_list.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/permissions_parser.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/user_script.h"

namespace extensions {

namespace {

// The entry into the ExtensionPrefs indicating that an extension should be
// granted all the requested host permissions without requiring explicit runtime
// permission from the user. The preference name is different for legacy
// reasons.
const char kGrantExtensionAllHostPermissionsPrefName[] =
    "extension_can_script_all_urls";

// Returns true if Chrome can potentially withhold permissions from the
// extension.
bool CanWithholdFromExtension(const Extension& extension) {
  // Some extensions must retain privilege to all requested host permissions.
  // Specifically, extensions that don't show up in chrome:extensions (where
  // withheld permissions couldn't be granted), extensions that are part of
  // chrome or corporate policy, and extensions that are whitelisted to script
  // everywhere must always have permission to run on a page.
  return extension.ShouldDisplayInExtensionSettings() &&
         !Manifest::IsPolicyLocation(extension.location()) &&
         !Manifest::IsComponentLocation(extension.location()) &&
         !PermissionsData::CanExecuteScriptEverywhere(extension.id(),
                                                      extension.location());
}

// Iterates over |requested_permissions| and adds any permissions that should
// be granted to |granted_permissions_out|. These include any non-host
// permissions or host permissions that are present in
// |runtime_granted_permissions|. |granted_permissions_out| may contain new
// patterns not found in either |requested_permissions| or
// |runtime_granted_permissions| in the case of overlapping host permissions
// (such as *://*.google.com/* and https://*/*, which would intersect with
// https://*.google.com/*).
void PartitionHostPermissions(
    const PermissionSet& requested_permissions,
    const PermissionSet& runtime_granted_permissions,
    std::unique_ptr<const PermissionSet>* granted_permissions_out) {
  auto segregate_url_permissions =
      [](const URLPatternSet& requested_patterns,
         const URLPatternSet& runtime_granted_patterns,
         URLPatternSet* granted) {
        *granted = URLPatternSet::CreateIntersection(
            requested_patterns, runtime_granted_patterns,
            URLPatternSet::IntersectionBehavior::kDetailed);
        for (const URLPattern& pattern : requested_patterns) {
          // The chrome://favicon permission is special. It is requested by
          // extensions to access stored favicons, but is not a traditional
          // host permission. Since it cannot be reasonably runtime-granted
          // while the user is on the site (i.e., the user never visits
          // chrome://favicon/), we auto-grant it and treat it like an API
          // permission.
          bool is_chrome_favicon =
              pattern.host() == "favicon" && pattern.scheme() == "chrome";
          if (is_chrome_favicon)
            granted->AddPattern(pattern);
        }
      };

  URLPatternSet granted_explicit_hosts;
  URLPatternSet granted_scriptable_hosts;
  segregate_url_permissions(requested_permissions.explicit_hosts(),
                            runtime_granted_permissions.explicit_hosts(),
                            &granted_explicit_hosts);
  segregate_url_permissions(requested_permissions.scriptable_hosts(),
                            runtime_granted_permissions.scriptable_hosts(),
                            &granted_scriptable_hosts);

  *granted_permissions_out = std::make_unique<PermissionSet>(
      requested_permissions.apis(),
      requested_permissions.manifest_permissions(), granted_explicit_hosts,
      granted_scriptable_hosts);
}

// Returns true if the extension should even be considered for being affected
// by the runtime host permissions experiment.
bool ShouldConsiderExtension(const Extension& extension) {
  // No extensions are affected if the experiment is disabled.
  if (!base::FeatureList::IsEnabled(
          extensions_features::kRuntimeHostPermissions))
    return false;

  // Certain extensions are always exempt from having permissions withheld.
  if (!CanWithholdFromExtension(extension))
    return false;

  return true;
}

base::Optional<bool> GetWithholdPermissionsPrefValue(
    const ExtensionPrefs& prefs,
    const ExtensionId& id) {
  bool permissions_allowed = false;
  if (!prefs.ReadPrefAsBoolean(id, kGrantExtensionAllHostPermissionsPrefName,
                               &permissions_allowed)) {
    return base::nullopt;
  }
  // NOTE: For legacy reasons, the preference stores whether the extension was
  // allowed access to all its host permissions, rather than if Chrome should
  // withhold permissions. Invert the boolean for backwards compatibility.
  return !permissions_allowed;
}

void SetWithholdPermissionsPrefValue(ExtensionPrefs* prefs,
                                     const ExtensionId& id,
                                     bool should_withhold) {
  // NOTE: For legacy reasons, the preference stores whether the extension was
  // allowed access to all its host permissions, rather than if Chrome should
  // withhold permissions. Invert the boolean for backwards compatibility.
  bool permissions_allowed = !should_withhold;
  prefs->UpdateExtensionPref(
      id, kGrantExtensionAllHostPermissionsPrefName,
      std::make_unique<base::Value>(permissions_allowed));
}

}  // namespace

ScriptingPermissionsModifier::ScriptingPermissionsModifier(
    content::BrowserContext* browser_context,
    const scoped_refptr<const Extension>& extension)
    : browser_context_(browser_context),
      extension_(extension),
      extension_prefs_(ExtensionPrefs::Get(browser_context_)) {
  DCHECK(extension_);
}

ScriptingPermissionsModifier::~ScriptingPermissionsModifier() {}

void ScriptingPermissionsModifier::SetWithholdHostPermissions(
    bool should_withhold) {
  DCHECK(CanAffectExtension());

  if (HasWithheldHostPermissions() == should_withhold)
    return;

  // Set the pref first, so that listeners for permission changes get the proper
  // value if they query HasWithheldHostPermissions().
  SetWithholdPermissionsPrefValue(extension_prefs_, extension_->id(),
                                  should_withhold);

  if (should_withhold)
    WithholdHostPermissions();
  else
    GrantWithheldHostPermissions();
}

bool ScriptingPermissionsModifier::HasWithheldHostPermissions() const {
  DCHECK(CanAffectExtension());

  base::Optional<bool> pref_value =
      GetWithholdPermissionsPrefValue(*extension_prefs_, extension_->id());
  if (!pref_value.has_value()) {
    // If there is no value present, default to false.
    return false;
  }
  return *pref_value;
}

bool ScriptingPermissionsModifier::CanAffectExtension() const {
  if (!ShouldConsiderExtension(*extension_))
    return false;

  // The extension can be affected if it currently has host permissions, or if
  // it did and they are actively withheld.
  return !extension_->permissions_data()
              ->active_permissions()
              .effective_hosts()
              .is_empty() ||
         !extension_->permissions_data()
              ->withheld_permissions()
              .effective_hosts()
              .is_empty();
}

void ScriptingPermissionsModifier::GrantHostPermission(const GURL& url) {
  DCHECK(CanAffectExtension());

  URLPatternSet explicit_hosts;
  explicit_hosts.AddOrigin(Extension::kValidHostPermissionSchemes, url);
  URLPatternSet scriptable_hosts;
  scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), url);

  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        explicit_hosts, scriptable_hosts));
}

bool ScriptingPermissionsModifier::HasGrantedHostPermission(
    const GURL& url) const {
  DCHECK(CanAffectExtension());

  GURL origin = url.GetOrigin();
  return extension_prefs_->GetRuntimeGrantedPermissions(extension_->id())
      ->effective_hosts()
      .MatchesURL(origin);
}

void ScriptingPermissionsModifier::RemoveGrantedHostPermission(
    const GURL& url) {
  DCHECK(CanAffectExtension());
  DCHECK(HasGrantedHostPermission(url));

  URLPatternSet explicit_hosts;
  explicit_hosts.AddOrigin(Extension::kValidHostPermissionSchemes, url);
  URLPatternSet scriptable_hosts;
  scriptable_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(), url);

  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(
          *extension_,
          PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                        explicit_hosts, scriptable_hosts));
}

void ScriptingPermissionsModifier::RemoveAllGrantedHostPermissions() {
  DCHECK(CanAffectExtension());
  WithholdHostPermissions();
}

// static
void ScriptingPermissionsModifier::WithholdPermissionsIfNecessary(
    const Extension& extension,
    const ExtensionPrefs& extension_prefs,
    const PermissionSet& permissions,
    std::unique_ptr<const PermissionSet>* granted_permissions_out) {
  bool should_withhold = false;
  if (ShouldConsiderExtension(extension)) {
    base::Optional<bool> pref_value =
        GetWithholdPermissionsPrefValue(extension_prefs, extension.id());
    should_withhold = pref_value.has_value() && pref_value.value() == true;
  }

  should_withhold &= !permissions.effective_hosts().is_empty();
  if (!should_withhold) {
    *granted_permissions_out = permissions.Clone();
    return;
  }

  // Only grant host permissions that the user has explicitly granted at
  // runtime through the runtime host permissions feature or the optional
  // permissions API.
  std::unique_ptr<const PermissionSet> runtime_granted_permissions =
      extension_prefs.GetRuntimeGrantedPermissions(extension.id());
  PartitionHostPermissions(permissions, *runtime_granted_permissions,
                           granted_permissions_out);
}

std::unique_ptr<const PermissionSet>
ScriptingPermissionsModifier::GetRevokablePermissions() const {
  // No extra revokable permissions if the extension couldn't ever be affected.
  if (!ShouldConsiderExtension(*extension_))
    return nullptr;

  // If we aren't withholding host permissions, then there may be some
  // permissions active on the extension that should be revokable. Otherwise,
  // all granted permissions should be stored in the preferences (and these
  // can be a superset of permissions on the extension, as in the case of e.g.
  // granting origins when only a subset is requested by the extension).
  // TODO(devlin): This is confusing and subtle. We should instead perhaps just
  // add all requested hosts as runtime-granted hosts if we aren't withholding
  // host permissions.
  const PermissionSet* current_granted_permissions = nullptr;
  std::unique_ptr<const PermissionSet> runtime_granted_permissions =
      extension_prefs_->GetRuntimeGrantedPermissions(extension_->id());
  std::unique_ptr<const PermissionSet> union_set;
  if (runtime_granted_permissions) {
    union_set = PermissionSet::CreateUnion(
        *runtime_granted_permissions,
        extension_->permissions_data()->active_permissions());
    current_granted_permissions = union_set.get();
  } else {
    current_granted_permissions =
        &extension_->permissions_data()->active_permissions();
  }

  // Revokable permissions are those that would be withheld if there were no
  // runtime-granted permissions.
  PermissionSet empty_runtime_granted_permissions;
  std::unique_ptr<const PermissionSet> granted_permissions;
  PartitionHostPermissions(*current_granted_permissions,
                           empty_runtime_granted_permissions,
                           &granted_permissions);
  return PermissionSet::CreateDifference(*current_granted_permissions,
                                         *granted_permissions);
}

void ScriptingPermissionsModifier::GrantWithheldHostPermissions() {
  const PermissionSet& withheld =
      extension_->permissions_data()->withheld_permissions();

  PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                            withheld.explicit_hosts(),
                            withheld.scriptable_hosts());
  PermissionsUpdater(browser_context_)
      .GrantRuntimePermissions(*extension_, permissions);
}

void ScriptingPermissionsModifier::WithholdHostPermissions() {
  PermissionsUpdater(browser_context_)
      .RevokeRuntimePermissions(*extension_, *GetRevokablePermissions());
}

}  // namespace extensions
