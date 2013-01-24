// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/accessibility_util.h"

#include <queue>

#include "ash/high_contrast/high_contrast_controller.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/accessibility/accessibility_extension_api.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/accessibility/magnification_manager.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/file_reader.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/speech/tts_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/extensions/user_script.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::RenderViewHost;

namespace chromeos {
namespace accessibility {

// Helper class that directly loads an extension's content scripts into
// all of the frames corresponding to a given RenderViewHost.
class ContentScriptLoader {
 public:
  // Initialize the ContentScriptLoader with the ID of the extension
  // and the RenderViewHost where the scripts should be loaded.
  ContentScriptLoader(const std::string& extension_id,
                      int render_process_id,
                      int render_view_id)
      : extension_id_(extension_id),
        render_process_id_(render_process_id),
        render_view_id_(render_view_id) {}

  // Call this once with the ExtensionResource corresponding to each
  // content script to be loaded.
  void AppendScript(ExtensionResource resource) {
    resources_.push(resource);
  }

  // Fianlly, call this method once to fetch all of the resources and
  // load them. This method will delete this object when done.
  void Run() {
    if (resources_.empty()) {
      delete this;
      return;
    }

    ExtensionResource resource = resources_.front();
    resources_.pop();
    scoped_refptr<FileReader> reader(new FileReader(resource, base::Bind(
        &ContentScriptLoader::OnFileLoaded, base::Unretained(this))));
    reader->Start();
  }

 private:
  void OnFileLoaded(bool success, const std::string& data) {
    if (success) {
      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = 0;
      params.extension_id = extension_id_;
      params.is_javascript = true;
      params.code = data;
      params.run_at = extensions::UserScript::DOCUMENT_IDLE;
      params.all_frames = true;
      params.in_main_world = false;

      RenderViewHost* render_view_host =
          RenderViewHost::FromID(render_process_id_, render_view_id_);
      if (render_view_host) {
        render_view_host->Send(new ExtensionMsg_ExecuteCode(
            render_view_host->GetRoutingID(), params));
      }
    }
    Run();
  }

  std::string extension_id_;
  int render_process_id_;
  int render_view_id_;
  std::queue<ExtensionResource> resources_;
};

void UpdateChromeOSAccessibilityHistograms() {
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosSpokenFeedback",
                        IsSpokenFeedbackEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosHighContrast",
                        IsHighContrastEnabled());
  UMA_HISTOGRAM_BOOLEAN("Accessibility.CrosVirtualKeyboard",
                        IsVirtualKeyboardEnabled());
  if (MagnificationManager::Get()) {
    uint32 type = MagnificationManager::Get()->IsMagnifierEnabled() ?
                      MagnificationManager::Get()->GetMagnifierType() : 0;
    // '0' means magnifier is disabled.
    UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosScreenMagnifier",
                              type,
                              ash::kMaxMagnifierType + 1);
  }
}

void Initialize() {
  content::BrowserAccessibilityState::GetInstance()->AddHistogramCallback(
      base::Bind(&UpdateChromeOSAccessibilityHistograms));
}

void EnableSpokenFeedback(bool enabled,
                          content::WebUI* login_web_ui,
                          ash::AccessibilityNotificationVisibility notify) {
  bool spoken_feedback_enabled = g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kSpokenFeedbackEnabled);
  if (spoken_feedback_enabled == enabled) {
    DLOG(INFO) << "Spoken feedback is already " <<
        (enabled ? "enabled" : "disabled") << ".  Going to do nothing.";
    return;
  }

  g_browser_process->local_state()->SetBoolean(
      prefs::kSpokenFeedbackEnabled, enabled);
  g_browser_process->local_state()->CommitPendingWrite();
  ExtensionAccessibilityEventRouter::GetInstance()->
      SetAccessibilityEnabled(enabled);

  AccessibilityStatusEventDetails details(enabled, notify);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_SPOKEN_FEEDBACK,
      content::NotificationService::AllSources(),
      content::Details<AccessibilityStatusEventDetails>(&details));

  Speak(l10n_util::GetStringUTF8(
      enabled ? IDS_CHROMEOS_ACC_SPOKEN_FEEDBACK_ENABLED :
      IDS_CHROMEOS_ACC_SPOKEN_FEEDBACK_DISABLED).c_str());

  // Load/Unload ChromeVox
  Profile* profile = ProfileManager::GetDefaultProfile();
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  FilePath path = FilePath(extension_misc::kChromeVoxExtensionPath);
  if (enabled) {  // Load ChromeVox
    std::string extension_id =
        extension_service->component_loader()->Add(IDR_CHROMEVOX_MANIFEST,
                                                   path);
    const extensions::Extension* extension =
        extension_service->extensions()->GetByID(extension_id);

    if (login_web_ui) {
      RenderViewHost* render_view_host =
          login_web_ui->GetWebContents()->GetRenderViewHost();
      // Set a flag to tell ChromeVox that it's just been enabled,
      // so that it won't interrupt our speech feedback enabled message.
      ExtensionMsg_ExecuteCode_Params params;
      params.request_id = 0;
      params.extension_id = extension->id();
      params.is_javascript = true;
      params.code = "window.INJECTED_AFTER_LOAD = true;";
      params.run_at = extensions::UserScript::DOCUMENT_IDLE;
      params.all_frames = true;
      params.in_main_world = false;
      render_view_host->Send(new ExtensionMsg_ExecuteCode(
          render_view_host->GetRoutingID(), params));

      // Inject ChromeVox' content scripts.
      ContentScriptLoader* loader = new ContentScriptLoader(
          extension->id(), render_view_host->GetProcess()->GetID(),
          render_view_host->GetRoutingID());

      for (size_t i = 0; i < extension->content_scripts().size(); i++) {
        const extensions::UserScript& script = extension->content_scripts()[i];
        for (size_t j = 0; j < script.js_scripts().size(); ++j) {
          const extensions::UserScript::File &file = script.js_scripts()[j];
          ExtensionResource resource = extension->GetResource(
              file.relative_path());
          loader->AppendScript(resource);
        }
      }
      loader->Run();  // It cleans itself up when done.
    }

    DLOG(INFO) << "ChromeVox was Loaded.";
  } else {  // Unload ChromeVox
    extension_service->component_loader()->Remove(path);
    DLOG(INFO) << "ChromeVox was Unloaded.";
  }
}

void EnableHighContrast(bool enabled) {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kHighContrastEnabled, enabled);
  pref_service->CommitPendingWrite();

  AccessibilityStatusEventDetails detail(enabled, ash::A11Y_NOTIFICATION_NONE);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_CROS_ACCESSIBILITY_TOGGLE_HIGH_CONTRAST_MODE,
      content::NotificationService::AllSources(),
      content::Details<AccessibilityStatusEventDetails>(&detail));

#if defined(USE_ASH)
  ash::Shell::GetInstance()->high_contrast_controller()->SetEnabled(enabled);
#endif
}

void EnableVirtualKeyboard(bool enabled) {
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kVirtualKeyboardEnabled, enabled);
  pref_service->CommitPendingWrite();
}

void ToggleSpokenFeedback(content::WebUI* login_web_ui,
    ash::AccessibilityNotificationVisibility notify) {
  bool spoken_feedback_enabled = g_browser_process &&
      g_browser_process->local_state()->GetBoolean(
          prefs::kSpokenFeedbackEnabled);
  spoken_feedback_enabled = !spoken_feedback_enabled;
  EnableSpokenFeedback(spoken_feedback_enabled, login_web_ui, notify);
};

void Speak(const std::string& text) {
  UtteranceContinuousParameters params;

  Profile* profile = ProfileManager::GetDefaultProfile();
  Utterance* utterance = new Utterance(profile);
  utterance->set_text(text);
  utterance->set_lang(g_browser_process->GetApplicationLocale());
  utterance->set_continuous_parameters(params);
  utterance->set_can_enqueue(false);
  utterance->set_options(new DictionaryValue());

  TtsController* controller = TtsController::GetInstance();
  controller->SpeakOrEnqueue(utterance);
}

bool IsSpokenFeedbackEnabled() {
  if (!g_browser_process) {
    return false;
  }
  PrefService* prefs = g_browser_process->local_state();
  bool spoken_feedback_enabled = prefs &&
      prefs->GetBoolean(prefs::kSpokenFeedbackEnabled);
  return spoken_feedback_enabled;
}

bool IsHighContrastEnabled() {
  if (!g_browser_process) {
    return false;
  }
  PrefService* prefs = g_browser_process->local_state();
  bool high_contrast_enabled = prefs &&
      prefs->GetBoolean(prefs::kHighContrastEnabled);
  return high_contrast_enabled;
}

bool IsVirtualKeyboardEnabled() {
  if (!g_browser_process) {
    return false;
  }
  PrefService* prefs = g_browser_process->local_state();
  bool virtual_keyboard_enabled = prefs &&
      prefs->GetBoolean(prefs::kVirtualKeyboardEnabled);
  return virtual_keyboard_enabled;
}

void MaybeSpeak(const std::string& utterance) {
  if (IsSpokenFeedbackEnabled())
    Speak(utterance);
}

void ShowAccessibilityHelp(Browser* browser) {
  chrome::ShowSingletonTab(browser, GURL(chrome::kChromeAccessibilityHelpURL));
}

}  // namespace accessibility
}  // namespace chromeos
