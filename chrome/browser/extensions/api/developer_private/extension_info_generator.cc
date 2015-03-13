// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/extension_info_generator.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/developer_private/inspectable_views_finder.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/path_util.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/extensions/extension_icon_source.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/warning_service.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/offline_enabled_info.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace developer = api::developer_private;

namespace {

// Given a Manifest::Type, converts it into its developer_private
// counterpart.
developer::ExtensionType GetExtensionType(Manifest::Type manifest_type) {
  developer::ExtensionType type = developer::EXTENSION_TYPE_EXTENSION;
  switch (manifest_type) {
    case Manifest::TYPE_EXTENSION:
      type = developer::EXTENSION_TYPE_EXTENSION;
      break;
    case Manifest::TYPE_THEME:
      type = developer::EXTENSION_TYPE_THEME;
      break;
    case Manifest::TYPE_HOSTED_APP:
      type = developer::EXTENSION_TYPE_HOSTED_APP;
      break;
    case Manifest::TYPE_LEGACY_PACKAGED_APP:
      type = developer::EXTENSION_TYPE_LEGACY_PACKAGED_APP;
      break;
    case Manifest::TYPE_PLATFORM_APP:
      type = developer::EXTENSION_TYPE_PLATFORM_APP;
      break;
    case Manifest::TYPE_SHARED_MODULE:
      type = developer::EXTENSION_TYPE_SHARED_MODULE;
      break;
    default:
      NOTREACHED();
  }
  return type;
}

// Populates the common fields of an extension error.
template <typename ErrorType>
void PopulateErrorBase(const ExtensionError& error,
                       ErrorType* out) {
  CHECK(out);
  out->type = error.type() == ExtensionError::MANIFEST_ERROR ?
      developer::ERROR_TYPE_MANIFEST : developer::ERROR_TYPE_RUNTIME;
  out->extension_id = error.extension_id();
  out->from_incognito = error.from_incognito();
  out->source = base::UTF16ToUTF8(error.source());
  out->message = base::UTF16ToUTF8(error.message());
}

// Given a ManifestError object, converts it into its developer_private
// counterpart.
linked_ptr<developer::ManifestError> ConstructManifestError(
    const ManifestError& error) {
  linked_ptr<developer::ManifestError> result(new developer::ManifestError());
  PopulateErrorBase(error, result.get());
  result->manifest_key = base::UTF16ToUTF8(error.manifest_key());
  if (!error.manifest_specific().empty()) {
    result->manifest_specific.reset(
        new std::string(base::UTF16ToUTF8(error.manifest_specific())));
  }
  return result;
}

// Given a RuntimeError object, converts it into its developer_private
// counterpart.
linked_ptr<developer::RuntimeError> ConstructRuntimeError(
    const RuntimeError& error) {
  linked_ptr<developer::RuntimeError> result(new developer::RuntimeError());
  PopulateErrorBase(error, result.get());
  switch (error.level()) {
    case logging::LOG_INFO:
      result->severity = developer::ERROR_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      result->severity = developer::ERROR_LEVEL_WARN;
      break;
    case logging::LOG_ERROR:
      result->severity = developer::ERROR_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  result->occurrences = error.occurrences();
  result->render_view_id = error.render_view_id();
  result->render_process_id = error.render_process_id();
  result->can_inspect =
      content::RenderViewHost::FromID(error.render_process_id(),
                                      error.render_view_id()) != nullptr;
  for (const StackFrame& f : error.stack_trace()) {
    linked_ptr<developer::StackFrame> frame(new developer::StackFrame());
    frame->line_number = f.line_number;
    frame->column_number = f.column_number;
    frame->url = base::UTF16ToUTF8(f.source);
    frame->function_name = base::UTF16ToUTF8(f.function);
    result->stack_trace.push_back(frame);
  }
  return result;
}

}  // namespace

ExtensionInfoGenerator::ExtensionInfoGenerator(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      extension_system_(ExtensionSystem::Get(browser_context)),
      extension_prefs_(ExtensionPrefs::Get(browser_context)),
      warning_service_(WarningService::Get(browser_context)),
      error_console_(ErrorConsole::Get(browser_context)) {
}

ExtensionInfoGenerator::~ExtensionInfoGenerator() {
}

scoped_ptr<developer::ExtensionInfo>
ExtensionInfoGenerator::CreateExtensionInfo(const Extension& extension,
                                            developer::ExtensionState state) {
  scoped_ptr<developer::ExtensionInfo> info(new developer::ExtensionInfo());

  // Don't consider the button hidden with the redesign, because "hidden"
  // buttons are now just hidden in the wrench menu.
  info->action_button_hidden =
      !ExtensionActionAPI::GetBrowserActionVisibility(
          extension_prefs_, extension.id()) &&
      !FeatureSwitch::extension_action_redesign()->IsEnabled();

  // Blacklist text.
  int blacklist_text = -1;
  switch (extension_prefs_->GetExtensionBlacklistState(extension.id())) {
    case BLACKLISTED_SECURITY_VULNERABILITY:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_SECURITY_VULNERABILITY;
      break;
    case BLACKLISTED_CWS_POLICY_VIOLATION:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_CWS_POLICY_VIOLATION;
      break;
    case BLACKLISTED_POTENTIALLY_UNWANTED:
      blacklist_text = IDS_OPTIONS_BLACKLISTED_POTENTIALLY_UNWANTED;
      break;
    default:
      break;
  }
  if (blacklist_text != -1) {
    info->blacklist_text.reset(
        new std::string(l10n_util::GetStringUTF8(blacklist_text)));
  }

  // Dependent extensions.
  if (extension.is_shared_module()) {
    scoped_ptr<ExtensionSet> dependent_extensions =
        extension_system_->extension_service()->
            shared_module_service()->GetDependentExtensions(&extension);
    for (const scoped_refptr<const Extension>& dependent :
             *dependent_extensions)
      info->dependent_extensions.push_back(dependent->id());
  }

  info->description = extension.description();

  // Disable reasons.
  int disable_reasons = extension_prefs_->GetDisableReasons(extension.id());
  info->disable_reasons.suspicious_install =
      (disable_reasons & Extension::DISABLE_NOT_VERIFIED) != 0;
  info->disable_reasons.corrupt_install =
      (disable_reasons & Extension::DISABLE_CORRUPTED) != 0;
  info->disable_reasons.update_required =
      (disable_reasons & Extension::DISABLE_UPDATE_REQUIRED_BY_POLICY) != 0;

  // Error collection.
  bool error_console_enabled =
      error_console_->IsEnabledForChromeExtensionsPage();
  info->error_collection.is_enabled = error_console_enabled;
  info->error_collection.is_active =
      error_console_enabled &&
      error_console_->IsReportingEnabledForExtension(extension.id());

  // File access.
  info->file_access.is_enabled = extension.wants_file_access();
  info->file_access.is_active =
      util::AllowFileAccess(extension.id(), browser_context_);

  // Home page.
  info->home_page.url = ManifestURL::GetHomepageURL(&extension).spec();
  info->home_page.specified = ManifestURL::SpecifiedHomepageURL(&extension);

  bool is_enabled = state == developer::EXTENSION_STATE_ENABLED;

  // TODO(devlin): This won't work with apps (CORS). We should convert to data
  // urls.
  info->icon_url =
      ExtensionIconSource::GetIconURL(&extension,
                                      extension_misc::EXTENSION_ICON_MEDIUM,
                                      ExtensionIconSet::MATCH_BIGGER,
                                      !is_enabled,
                                      nullptr).spec();

  info->id = extension.id();

  // Incognito access.
  info->incognito_access.is_enabled = extension.can_be_incognito_enabled();
  info->incognito_access.is_active =
      util::IsIncognitoEnabled(extension.id(), browser_context_);

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  info->installed_by_custodian =
      util::IsExtensionSupervised(&extension, profile);

  // Install warnings (only if unpacked and no error console).
  if (!error_console_enabled &&
      Manifest::IsUnpackedLocation(extension.location())) {
    const std::vector<InstallWarning>& install_warnings =
        extension.install_warnings();
    for (const InstallWarning& warning : install_warnings)
      info->install_warnings.push_back(warning.message);
  }

  // Launch url.
  if (extension.is_app()) {
    info->launch_url.reset(
        new std::string(AppLaunchInfo::GetFullLaunchURL(&extension).spec()));
  }

  // Location.
  if (extension.location() == Manifest::INTERNAL &&
      ManifestURL::UpdatesFromGallery(&extension)) {
    info->location = developer::LOCATION_FROM_STORE;
  } else if (Manifest::IsUnpackedLocation(extension.location())) {
    info->location = developer::LOCATION_UNPACKED;
  } else if (Manifest::IsExternalLocation(extension.location()) &&
             ManifestURL::UpdatesFromGallery(&extension)) {
    info->location = developer::LOCATION_THIRD_PARTY;
  } else {
    info->location = developer::LOCATION_UNKNOWN;
  }

  // Location text.
  int location_text = -1;
  if (info->location == developer::LOCATION_UNKNOWN)
    location_text = IDS_OPTIONS_INSTALL_LOCATION_UNKNOWN;
  else if (extension.location() == Manifest::EXTERNAL_REGISTRY)
    location_text = IDS_OPTIONS_INSTALL_LOCATION_3RD_PARTY;
  else if (extension.is_shared_module())
    location_text = IDS_OPTIONS_INSTALL_LOCATION_SHARED_MODULE;
  if (location_text != -1) {
    info->location_text.reset(
        new std::string(l10n_util::GetStringUTF8(location_text)));
  }

  // Runtime/Manifest errors.
  if (error_console_enabled) {
    const ErrorList& errors =
        error_console_->GetErrorsForExtension(extension.id());
    for (const ExtensionError* error : errors) {
      switch (error->type()) {
        case ExtensionError::MANIFEST_ERROR:
          info->manifest_errors.push_back(ConstructManifestError(
              static_cast<const ManifestError&>(*error)));
          break;
        case ExtensionError::RUNTIME_ERROR:
          info->runtime_errors.push_back(ConstructRuntimeError(
              static_cast<const RuntimeError&>(*error)));
          break;
        case ExtensionError::NUM_ERROR_TYPES:
          NOTREACHED();
          break;
      }
    }
  }

  ManagementPolicy* management_policy = extension_system_->management_policy();
  info->must_remain_installed =
      management_policy->MustRemainInstalled(&extension, nullptr);

  info->name = extension.name();
  info->offline_enabled = OfflineEnabledInfo::IsOfflineEnabled(&extension);

  // Options page.
  if (OptionsPageInfo::HasOptionsPage(&extension)) {
    info->options_page.reset(new developer::OptionsPage());
    info->options_page->open_in_tab =
        OptionsPageInfo::ShouldOpenInTab(&extension);
    info->options_page->url =
        OptionsPageInfo::GetOptionsPage(&extension).spec();
  }

  // Path.
  if (Manifest::IsUnpackedLocation(extension.location())) {
    info->path.reset(new std::string(extension.path().AsUTF8Unsafe()));
    info->prettified_path.reset(new std::string(
      extensions::path_util::PrettifyPath(extension.path()).AsUTF8Unsafe()));
  }

  if (Manifest::IsPolicyLocation(extension.location())) {
    info->policy_text.reset(new std::string(
        l10n_util::GetStringUTF8(IDS_OPTIONS_INSTALL_LOCATION_ENTERPRISE)));
  }

  // Runs on all urls.
  info->run_on_all_urls.is_enabled =
      (FeatureSwitch::scripts_require_action()->IsEnabled() &&
       PermissionsData::ScriptsMayRequireActionForExtension(
           &extension,
           extension.permissions_data()->active_permissions().get())) ||
      extension.permissions_data()->HasWithheldImpliedAllHosts() ||
      util::HasSetAllowedScriptingOnAllUrls(extension.id(), browser_context_);
  info->run_on_all_urls.is_active =
      util::AllowedScriptingOnAllUrls(extension.id(), browser_context_);

  // Runtime warnings.
  std::vector<std::string> warnings =
      warning_service_->GetWarningMessagesForExtension(extension.id());
  for (const std::string& warning : warnings)
    info->runtime_warnings.push_back(warning);

  info->state = state;

  info->type = GetExtensionType(extension.manifest()->type());

  info->update_url = ManifestURL::GetUpdateURL(&extension).spec();

  info->user_may_modify =
      management_policy->UserMayModifySettings(&extension, nullptr);

  info->version = extension.GetVersionForDisplay();

  if (state != developer::EXTENSION_STATE_TERMINATED) {
    info->views = InspectableViewsFinder(profile, nullptr).
                      GetViewsForExtension(extension, is_enabled);
  }
  return info.Pass();
}

ExtensionInfoGenerator::ExtensionInfoList
ExtensionInfoGenerator::CreateExtensionsInfo(bool include_disabled,
                                             bool include_terminated) {
  std::vector<linked_ptr<developer::ExtensionInfo>> list;
  auto add_to_list = [this, &list](const ExtensionSet& extensions,
                                      developer::ExtensionState state) {
    for (const scoped_refptr<const Extension>& extension : extensions) {
      if (ui_util::ShouldDisplayInExtensionSettings(extension.get(),
                                                    browser_context_)) {
        scoped_ptr<developer::ExtensionInfo> info =
            CreateExtensionInfo(*extension, state);
        list.push_back(make_linked_ptr(info.release()));
      }
    }
  };

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  add_to_list(registry->enabled_extensions(),
              developer::EXTENSION_STATE_ENABLED);
  if (include_disabled) {
    add_to_list(registry->disabled_extensions(),
                developer::EXTENSION_STATE_DISABLED);
  }
  if (include_terminated) {
    add_to_list(registry->terminated_extensions(),
                developer::EXTENSION_STATE_TERMINATED);
  }

  return list;
}

}  // namespace extensions
