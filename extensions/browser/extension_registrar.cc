// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registrar.h"

#include "base/logging.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {

ExtensionRegistrar::ExtensionRegistrar(content::BrowserContext* browser_context,
                                       Delegate* delegate)
    : browser_context_(browser_context),
      delegate_(delegate),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      registry_(ExtensionRegistry::Get(browser_context)),
      renderer_helper_(
          RendererStartupHelperFactory::GetForBrowserContext(browser_context)),
      weak_factory_(this) {}

ExtensionRegistrar::~ExtensionRegistrar() = default;

void ExtensionRegistrar::AddExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(nullptr, registry_->GetInstalledExtension(extension->id()));

  if (extension_prefs_->IsExtensionBlacklisted(extension->id())) {
    // Only prefs is checked for the blacklist. We rely on callers to check the
    // blacklist before calling into here, e.g. CrxInstaller checks before
    // installation then threads through the install and pending install flow
    // of this class, and ExtensionService checks when loading installed
    // extensions.
    registry_->AddBlacklisted(extension);
  } else if (delegate_->ShouldBlockExtension(extension.get())) {
    registry_->AddBlocked(extension);
  } else if (extension_prefs_->IsExtensionDisabled(extension->id())) {
    registry_->AddDisabled(extension);
    // Notify that a disabled extension was added or updated.
    content::NotificationService::current()->Notify(
        extensions::NOTIFICATION_EXTENSION_UPDATE_DISABLED,
        content::Source<content::BrowserContext>(browser_context_),
        content::Details<const Extension>(extension.get()));
  } else {  // Extension should be enabled.
    // All apps that are displayed in the launcher are ordered by their ordinals
    // so we must ensure they have valid ordinals.
    if (extension->RequiresSortOrdinal()) {
      AppSorting* app_sorting = extension_system_->app_sorting();
      app_sorting->SetExtensionVisible(extension->id(),
                                       extension->ShouldDisplayInNewTabPage());
      app_sorting->EnsureValidOrdinals(extension->id(),
                                       syncer::StringOrdinal());
    }
    registry_->AddEnabled(extension);
    ActivateExtension(extension.get(), true);
  }
}

void ExtensionRegistrar::RemoveExtension(const ExtensionId& extension_id,
                                         UnloadedExtensionReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::TERMINATED;
  scoped_refptr<const Extension> extension(
      registry_->GetExtensionById(extension_id, include_mask));

  // If the extension was already removed, just notify of the new unload reason.
  // TODO: It's unclear when this needs to be called given that it may be a
  // duplicate notification. See crbug.com/708230.
  if (!extension) {
    extension_system_->UnregisterExtensionWithRequestContexts(extension_id,
                                                              reason);
    return;
  }

  if (registry_->disabled_extensions().Contains(extension_id)) {
    // The extension is already deactivated.
    registry_->RemoveDisabled(extension->id());
    extension_system_->UnregisterExtensionWithRequestContexts(extension_id,
                                                              reason);
  } else {
    // TODO(michaelpg): The extension may be blocked or blacklisted, in which
    // case it shouldn't need to be "deactivated". Determine whether the removal
    // notifications are necessary (crbug.com/708230).
    registry_->RemoveEnabled(extension_id);
    DeactivateExtension(extension.get(), reason);
  }

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));
}

void ExtensionRegistrar::EnableExtension(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // First, check that the extension can be enabled.
  if (IsExtensionEnabled(extension_id) ||
      extension_prefs_->IsExtensionBlacklisted(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return;
  }

  const Extension* extension =
      registry_->disabled_extensions().GetByID(extension_id);
  if (extension && !delegate_->CanEnableExtension(extension))
    return;

  // Now that we know the extension can be enabled, update the prefs.
  extension_prefs_->SetExtensionEnabled(extension_id);

  // This can happen if sync enables an extension that is not installed yet.
  if (!extension)
    return;

  // Actually enable the extension.
  registry_->AddEnabled(extension);
  registry_->RemoveDisabled(extension->id());
  ActivateExtension(extension, false);
}

void ExtensionRegistrar::DisableExtension(const ExtensionId& extension_id,
                                          int disable_reasons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_NE(disable_reason::DISABLE_NONE, disable_reasons);

  if (extension_prefs_->IsExtensionBlacklisted(extension_id))
    return;

  // The extension may have been disabled already. Just add the disable reasons.
  // TODO(michaelpg): Move this after the policy check, below, to ensure that
  // disable reasons disallowed by policy are not added here.
  if (!IsExtensionEnabled(extension_id)) {
    extension_prefs_->AddDisableReasons(extension_id, disable_reasons);
    return;
  }

  scoped_refptr<const Extension> extension =
      registry_->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);

  bool is_controlled_extension =
      !delegate_->CanDisableExtension(extension.get());

  if (is_controlled_extension) {
    // Remove disallowed disable reasons.
    // Certain disable reasons are always allowed, since they are more internal
    // to the browser (rather than the user choosing to disable the extension).
    int internal_disable_reason_mask =
        extensions::disable_reason::DISABLE_RELOAD |
        extensions::disable_reason::DISABLE_CORRUPTED |
        extensions::disable_reason::DISABLE_UPDATE_REQUIRED_BY_POLICY |
        extensions::disable_reason::DISABLE_BLOCKED_BY_POLICY;
    disable_reasons &= internal_disable_reason_mask;

    if (disable_reasons == disable_reason::DISABLE_NONE)
      return;
  }

  extension_prefs_->SetExtensionDisabled(extension_id, disable_reasons);

  int include_mask =
      ExtensionRegistry::EVERYTHING & ~ExtensionRegistry::DISABLED;
  extension = registry_->GetExtensionById(extension_id, include_mask);
  if (!extension)
    return;

  // The extension is either enabled or terminated.
  DCHECK(registry_->enabled_extensions().Contains(extension->id()) ||
         registry_->terminated_extensions().Contains(extension->id()));

  // Move the extension to the disabled list.
  registry_->AddDisabled(extension);
  if (registry_->enabled_extensions().Contains(extension->id())) {
    registry_->RemoveEnabled(extension->id());
    DeactivateExtension(extension.get(), UnloadedExtensionReason::DISABLE);
  } else {
    // The extension must have been terminated. Don't send additional
    // notifications for it being disabled.
    bool removed = registry_->RemoveTerminated(extension->id());
    DCHECK(removed);
  }
}

bool ExtensionRegistrar::ReplaceReloadedExtension(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The extension must already be disabled, and the original extension has
  // been unloaded.
  CHECK(registry_->disabled_extensions().Contains(extension->id()));
  if (!delegate_->CanEnableExtension(extension.get()))
    return false;

  // TODO(michaelpg): Other disable reasons might have been added after the
  // reload started. We may want to keep the extension disabled and just remove
  // the DISABLE_RELOAD reason in that case.
  extension_prefs_->SetExtensionEnabled(extension->id());

  // Move it over to the enabled list.
  CHECK(registry_->RemoveDisabled(extension->id()));
  CHECK(registry_->AddEnabled(extension));

  ActivateExtension(extension.get(), false);

  return true;
}

void ExtensionRegistrar::TerminateExtension(const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<const Extension> extension =
      registry_->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return;

  registry_->AddTerminated(extension);
  registry_->RemoveEnabled(extension_id);
  DeactivateExtension(extension.get(), UnloadedExtensionReason::TERMINATE);
}

void ExtensionRegistrar::UntrackTerminatedExtension(
    const ExtensionId& extension_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  scoped_refptr<const Extension> extension =
      registry_->terminated_extensions().GetByID(extension_id);
  if (!extension)
    return;

  registry_->RemoveTerminated(extension_id);

  // TODO(michaelpg): This notification was already sent when the extension was
  // unloaded as part of being terminated. But we send it again as observers
  // may be tracking the terminated extension. See crbug.com/708230.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_EXTENSION_REMOVED,
      content::Source<content::BrowserContext>(browser_context_),
      content::Details<const Extension>(extension.get()));
}

bool ExtensionRegistrar::IsExtensionEnabled(
    const ExtensionId& extension_id) const {
  if (registry_->enabled_extensions().Contains(extension_id) ||
      registry_->terminated_extensions().Contains(extension_id)) {
    return true;
  }

  if (registry_->disabled_extensions().Contains(extension_id) ||
      registry_->blacklisted_extensions().Contains(extension_id) ||
      registry_->blocked_extensions().Contains(extension_id)) {
    return false;
  }

  if (delegate_->ShouldBlockExtension(nullptr))
    return false;

  // If the extension hasn't been loaded yet, check the prefs for it. Assume
  // enabled unless otherwise noted.
  return !extension_prefs_->IsExtensionDisabled(extension_id) &&
         !extension_prefs_->IsExtensionBlacklisted(extension_id) &&
         !extension_prefs_->IsExternalExtensionUninstalled(extension_id);
}

void ExtensionRegistrar::ActivateExtension(const Extension* extension,
                                           bool is_newly_added) {
  // The URLRequestContexts need to be first to know that the extension
  // was loaded. Otherwise a race can arise where a renderer that is created
  // for the extension may try to load an extension URL with an extension id
  // that the request context doesn't yet know about. The BrowserContext should
  // ensure its URLRequestContexts appropriately discover the loaded extension.
  extension_system_->RegisterExtensionWithRequestContexts(
      extension,
      base::Bind(&ExtensionRegistrar::OnExtensionRegisteredWithRequestContexts,
                 weak_factory_.GetWeakPtr(), WrapRefCounted(extension)));

  renderer_helper_->OnExtensionLoaded(*extension);

  // Tell subsystems that use the ExtensionRegistryObserver::OnExtensionLoaded
  // about the new extension.
  //
  // NOTE: It is important that this happen after notifying the renderers about
  // the new extensions so that if we navigate to an extension URL in
  // ExtensionRegistryObserver::OnExtensionLoaded the renderer is guaranteed to
  // know about it.
  registry_->TriggerOnLoaded(extension);

  delegate_->PostActivateExtension(extension, is_newly_added);
}

void ExtensionRegistrar::DeactivateExtension(const Extension* extension,
                                             UnloadedExtensionReason reason) {
  registry_->TriggerOnUnloaded(extension, reason);
  renderer_helper_->OnExtensionUnloaded(*extension);
  extension_system_->UnregisterExtensionWithRequestContexts(extension->id(),
                                                            reason);
  delegate_->PostDeactivateExtension(extension);
}

void ExtensionRegistrar::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<const Extension> extension) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  registry_->AddReady(extension);
  if (registry_->enabled_extensions().Contains(extension->id()))
    registry_->TriggerOnReady(extension.get());
}

}  // namespace extensions
