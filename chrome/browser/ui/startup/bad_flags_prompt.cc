// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/bad_flags_prompt.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/switch_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "media/base/media_switches.h"
#include "media/media_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace chrome {

namespace {

void ShowBadFlagsInfoBar(content::WebContents* web_contents,
                         int message_id,
                         const char* flag) {
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(flag);
  if (!switch_value.empty())
    switch_value = "=" + switch_value;
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      infobars::InfoBarDelegate::BAD_FLAGS_PROMPT, nullptr,
      l10n_util::GetStringFUTF16(
          message_id,
          base::UTF8ToUTF16(std::string("--") + flag + switch_value)),
      false);
}

}  // namespace

void ShowBadFlagsPrompt(Browser* browser) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return;

  // Flags only available in specific builds, for which to display a warning
  // "the flag is not implemented in this build", if necessary.
  struct {
    const char* name;
    bool is_invalid;
  } conditional_flags[] = {
      {switches::kEnableHeapProfiling,
       base::trace_event::MemoryDumpManager::
               GetHeapProfilingModeFromCommandLine() ==
           base::trace_event::kHeapProfilingModeInvalid},
  };
  for (auto conditional_flag : conditional_flags) {
    if (conditional_flag.is_invalid) {
      ShowBadFlagsInfoBar(web_contents, IDS_UNIMPLEMENTED_FLAGS_WARNING_MESSAGE,
                          conditional_flag.name);
      return;
    }
  }

  // Unsupported flags for which to display a warning that "stability and
  // security will suffer".
  static const char* kBadFlags[] = {
    // These flags disable sandbox-related security.
    switches::kDisableGpuSandbox,
    switches::kDisableSeccompFilterSandbox,
    switches::kDisableSetuidSandbox,
    switches::kDisableWebSecurity,
#if !defined(DISABLE_NACL)
    switches::kNaClDangerousNoSandboxNonSfi,
#endif
    switches::kNoSandbox,
    switches::kSingleProcess,

    // These flags disable or undermine the Same Origin Policy.
    translate::switches::kTranslateSecurityOrigin,

    // These flags undermine HTTPS / connection security.
#if BUILDFLAG(ENABLE_WEBRTC)
    switches::kDisableWebRtcEncryption,
#endif
    switches::kIgnoreCertificateErrors,
    switches::kIgnoreCertificateErrorsSPKIList,
    switches::kReduceSecurityForTesting,
    invalidation::switches::kSyncAllowInsecureXmppConnection,

    // These flags change the URLs that handle PII.
    switches::kGaiaUrl,
    translate::switches::kTranslateScriptURL,

    // This flag gives extensions more powers.
    extensions::switches::kExtensionsOnChromeURLs,

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // Speech dispatcher is buggy, it can crash and it can make Chrome freeze.
    // http://crbug.com/327295
    switches::kEnableSpeechDispatcher,
#endif

    // These flags control Blink feature state, which is not supported and is
    // intended only for use by Chromium developers.
    switches::kDisableBlinkFeatures,
    switches::kEnableBlinkFeatures,

    // This flag allows people to whitelist certain origins as secure, even
    // if they are not.
    switches::kUnsafelyTreatInsecureOriginAsSecure,

    // This flag allows sites to access the camera and microphone without
    // getting the user's permission.
    switches::kUseFakeUIForMediaStream,

    // This flag allows sites to access protected media identifiers without
    // getting the user's permission.
    switches::kUnsafelyAllowProtectedMediaIdentifierForDomain
  };

  for (const char* flag : kBadFlags) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(flag)) {
      ShowBadFlagsInfoBar(web_contents, IDS_BAD_FLAGS_WARNING_MESSAGE, flag);
      return;
    }
  }
}

void MaybeShowInvalidUserDataDirWarningDialog() {
  const base::FilePath& user_data_dir = GetInvalidSpecifiedUserDataDir();
  if (user_data_dir.empty())
    return;

  startup_metric_utils::SetNonBrowserUIDisplayed();

  // Ensure the ResourceBundle is initialized for string resource access.
  bool cleanup_resource_bundle = false;
  if (!ResourceBundle::HasSharedInstance()) {
    cleanup_resource_bundle = true;
    std::string locale = l10n_util::GetApplicationLocale(std::string());
    const char kUserDataDirDialogFallbackLocale[] = "en-US";
    if (locale.empty())
      locale = kUserDataDirDialogFallbackLocale;
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  const base::string16& title =
      l10n_util::GetStringUTF16(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
  const base::string16& message =
      l10n_util::GetStringFUTF16(IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY,
                                 user_data_dir.LossyDisplayName());

  if (cleanup_resource_bundle)
    ResourceBundle::CleanupSharedInstance();

  // More complex dialogs cannot be shown before the earliest calls here.
  ShowWarningMessageBox(NULL, title, message);
}

}  // namespace chrome
