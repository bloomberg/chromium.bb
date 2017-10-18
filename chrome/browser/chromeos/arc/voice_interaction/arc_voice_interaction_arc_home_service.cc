// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_arc_home_service.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/scale_utility.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/instance_holder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/platform/ax_snapshot_node_android_platform.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/dip_util.h"
#include "ui/snapshot/snapshot.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace arc {

namespace {

constexpr base::TimeDelta kAssistantStartedTimeout =
    base::TimeDelta::FromMinutes(1);
constexpr base::TimeDelta kWizardCompletedTimeout =
    base::TimeDelta::FromMinutes(1);

mojom::VoiceInteractionStructurePtr CreateVoiceInteractionStructure(
    const ui::AXSnapshotNodeAndroid& view_structure) {
  auto structure = mojom::VoiceInteractionStructure::New();
  structure->text = view_structure.text;
  structure->text_size = view_structure.text_size;

  structure->bold = view_structure.bold;
  structure->italic = view_structure.italic;
  structure->underline = view_structure.underline;
  structure->line_through = view_structure.line_through;
  structure->color = view_structure.color;
  structure->bgcolor = view_structure.bgcolor;

  structure->role = view_structure.role;

  structure->class_name = view_structure.class_name;
  structure->rect = view_structure.rect;

  if (view_structure.has_selection) {
    structure->selection = gfx::Range(view_structure.start_selection,
                                      view_structure.end_selection);
  }

  for (auto& child : view_structure.children)
    structure->children.push_back(CreateVoiceInteractionStructure(*child));

  return structure;
}

void RequestVoiceInteractionStructureCallback(
    ArcVoiceInteractionArcHomeService::GetVoiceInteractionStructureCallback
        callback,
    const gfx::Rect& bounds,
    const std::string& web_url,
    const base::string16& title,
    const ui::AXTreeUpdate& update) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The assist structure starts with 2 dummy nodes: Url node and title
  // node. Then we attach all nodes in view hierarchy.
  auto root = mojom::VoiceInteractionStructure::New();
  root->rect = bounds;
  root->class_name = "android.view.dummy.root.WebUrl";
  root->text = base::UTF8ToUTF16(web_url);

  auto title_node = mojom::VoiceInteractionStructure::New();
  title_node->rect = gfx::Rect(bounds.size());
  title_node->class_name = "android.view.dummy.WebTitle";
  title_node->text = title;
  title_node->children.push_back(CreateVoiceInteractionStructure(
      *ui::AXSnapshotNodeAndroid::Create(update, false)));
  root->children.push_back(std::move(title_node));
  std::move(callback).Run(std::move(root));
}

// Singleton factory for ArcVoiceInteractionArcHomeService.
class ArcVoiceInteractionArcHomeServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVoiceInteractionArcHomeService,
          ArcVoiceInteractionArcHomeServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName =
      "ArcVoiceInteractionArcHomeServiceFactory";

  static ArcVoiceInteractionArcHomeServiceFactory* GetInstance() {
    return base::Singleton<ArcVoiceInteractionArcHomeServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcVoiceInteractionArcHomeServiceFactory>;

  ArcVoiceInteractionArcHomeServiceFactory() {
    DependsOn(ArcAppListPrefsFactory::GetInstance());
    DependsOn(ArcVoiceInteractionFrameworkService::GetFactory());
  }
  ~ArcVoiceInteractionArcHomeServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    if (!chromeos::switches::IsVoiceInteractionEnabled())
      return nullptr;
    return ArcBrowserContextKeyedServiceFactoryBase::BuildServiceInstanceFor(
        context);
  }
};

}  // namespace

// static
const char ArcVoiceInteractionArcHomeService::kAssistantPackageName[] =
    "com.google.android.googlequicksearchbox";

// static
ArcVoiceInteractionArcHomeService*
ArcVoiceInteractionArcHomeService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVoiceInteractionArcHomeServiceFactory::GetForBrowserContext(
      context);
}

ArcVoiceInteractionArcHomeService::ArcVoiceInteractionArcHomeService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      assistant_started_timeout_(kAssistantStartedTimeout),
      wizard_completed_timeout_(kWizardCompletedTimeout),
      binding_(this) {
  arc_bridge_service_->voice_interaction_arc_home()->AddObserver(this);
  ArcSessionManager::Get()->AddObserver(this);
}

ArcVoiceInteractionArcHomeService::~ArcVoiceInteractionArcHomeService() =
    default;

void ArcVoiceInteractionArcHomeService::Shutdown() {
  ResetTimeouts();
  arc_bridge_service_->voice_interaction_arc_home()->RemoveObserver(this);
  ArcSessionManager::Get()->RemoveObserver(this);
}

void ArcVoiceInteractionArcHomeService::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  if (!pending_pai_lock_)
    return;

  pending_pai_lock_ = false;
  LockPai();
}

void ArcVoiceInteractionArcHomeService::LockPai() {
  ResetTimeouts();
  arc::ArcPaiStarter* pai_starter =
      arc::ArcSessionManager::Get()->pai_starter();
  if (!pai_starter) {
    DLOG(ERROR) << "There is no PAI starter.";
    // We could be starting before ARC session is started when user initiated
    // voice interaction first before ARC is enabled. We will remember this
    // and wait for ARC session started to try locking again.
    pending_pai_lock_ = true;
    return;
  }
  pai_starter->AcquireLock();
}

void ArcVoiceInteractionArcHomeService::UnlockPai() {
  ResetTimeouts();
  arc::ArcPaiStarter* pai_starter =
      arc::ArcSessionManager::Get()->pai_starter();
  if (!pai_starter || !pai_starter->locked())
    return;
  pai_starter->ReleaseLock();
}

void ArcVoiceInteractionArcHomeService::OnAssistantStarted() {
  VLOG(1) << "Assistant flow started";
  LockPai();
}

void ArcVoiceInteractionArcHomeService::OnAssistantAppRequested() {
  VLOG(1) << "Assistant app start request";

  ResetTimeouts();

  ArcAppListPrefs::Get(context_)->AddObserver(this);

  assistant_started_timer_.Start(
      FROM_HERE, assistant_started_timeout_,
      base::Bind(&ArcVoiceInteractionArcHomeService::OnAssistantStartTimeout,
                 base::Unretained(this)));
}

void ArcVoiceInteractionArcHomeService::OnAssistantCanceled() {
  VLOG(1) << "Assistant flow canceled";
  UnlockPai();
}

void ArcVoiceInteractionArcHomeService::OnTaskCreated(
    int32_t task_id,
    const std::string& package_name,
    const std::string& activity,
    const std::string& intent) {
  if (package_name != kAssistantPackageName)
    return;

  VLOG(1) << "Assistant app created";

  DCHECK_EQ(-1, assistant_task_id_);
  assistant_task_id_ = task_id;
  assistant_started_timer_.Stop();
}

void ArcVoiceInteractionArcHomeService::OnTaskDestroyed(int32_t task_id) {
  if (task_id != assistant_task_id_)
    return;

  VLOG(1) << "Assistant app exited";

  ResetTimeouts();
  wizard_completed_timer_.Start(
      FROM_HERE, wizard_completed_timeout_,
      base::Bind(&ArcVoiceInteractionArcHomeService::OnWizardCompleteTimeout,
                 base::Unretained(this)));
}

void ArcVoiceInteractionArcHomeService::ResetTimeouts() {
  ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(context_);
  if (arc_prefs)
    arc_prefs->RemoveObserver(this);

  assistant_started_timer_.Stop();
  wizard_completed_timer_.Stop();
}

void ArcVoiceInteractionArcHomeService::OnAssistantStartTimeout() {
  LOG(WARNING) << "Failed to start Assistant app.";
  UnlockPai();
}

void ArcVoiceInteractionArcHomeService::OnWizardCompleteTimeout() {
  LOG(WARNING) << "Assistant app was not completed successfully.";
  UnlockPai();
}

void ArcVoiceInteractionArcHomeService::OnInstanceReady() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojom::VoiceInteractionArcHomeInstance* home_instance =
      ARC_GET_INSTANCE_FOR_METHOD(
          arc_bridge_service_->voice_interaction_arc_home(), Init);
  DCHECK(home_instance);
  mojom::VoiceInteractionArcHomeHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  home_instance->Init(std::move(host_proxy));
}

void ArcVoiceInteractionArcHomeService::OnInstanceClosed() {
  VLOG(1) << "Voice interaction instance is closed.";
  UnlockPai();
}

void ArcVoiceInteractionArcHomeService::GetVoiceInteractionStructure(
    GetVoiceInteractionStructureCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PrefService* prefs = Profile::FromBrowserContext(context_)->GetPrefs();
  auto* framework_service =
      ArcVoiceInteractionFrameworkService::GetForBrowserContext(context_);
  if (!framework_service->ValidateTimeSinceUserInteraction() ||
      !prefs->GetBoolean(prefs::kVoiceInteractionEnabled) ||
      !prefs->GetBoolean(prefs::kVoiceInteractionContextEnabled)) {
    std::move(callback).Run(mojom::VoiceInteractionStructure::New());
    return;
  }
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || !browser->window()->IsActive()) {
    // TODO(muyuanli): retrieve context for apps.
    LOG(ERROR) << "Retrieving context from apps is not implemented.";
    std::move(callback).Run(mojom::VoiceInteractionStructure::New());
    return;
  }

  VLOG(1) << "Retrieving voice interaction context";

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Do not process incognito tab.
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    std::move(callback).Run(mojom::VoiceInteractionStructure::New());
    return;
  }

  auto transform = browser->window()
                       ->GetNativeWindow()
                       ->GetRootWindow()
                       ->GetHost()
                       ->GetRootTransform();
  float scale_factor = ash::GetScaleFactorForTransform(transform);
  web_contents->RequestAXTreeSnapshot(base::Bind(
      &RequestVoiceInteractionStructureCallback,
      base::Passed(std::move(callback)),
      gfx::ConvertRectToPixel(scale_factor, browser->window()->GetBounds()),
      web_contents->GetLastCommittedURL().spec(), web_contents->GetTitle()));
}

void ArcVoiceInteractionArcHomeService::OnVoiceInteractionOobeSetupComplete() {
  VLOG(1) << "Assistant wizard is completed.";
  UnlockPai();
  chromeos::first_run::MaybeLaunchDialogImmediately();
}

// static
mojom::VoiceInteractionStructurePtr
ArcVoiceInteractionArcHomeService::CreateVoiceInteractionStructureForTesting(
    const ui::AXSnapshotNodeAndroid& view_structure) {
  return CreateVoiceInteractionStructure(view_structure);
}

}  // namespace arc
