// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_arc_home_service.h"

#include <utility>
#include <vector>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/voice_interaction/arc_voice_interaction_framework_service.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/ui/app_list/arc/arc_pai_starter.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chromeos/chromeos_switches.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/instance_holder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/accessibility/platform/ax_snapshot_node_android_platform.h"
#include "ui/aura/window.h"
#include "ui/snapshot/snapshot.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

namespace arc {

namespace {

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

  structure->class_name = view_structure.class_name;
  structure->rect = view_structure.rect;

  if (view_structure.has_selection) {
    auto selection = mojom::TextSelection::New();
    selection->start_selection = view_structure.start_selection;
    selection->end_selection = view_structure.end_selection;
    structure->selection = std::move(selection);
  }

  for (auto& child : view_structure.children)
    structure->children.push_back(CreateVoiceInteractionStructure(*child));

  return structure;
}

void RequestVoiceInteractionStructureCallback(
    const base::Callback<void(mojom::VoiceInteractionStructurePtr)>& callback,
    const gfx::Rect& bounds,
    const std::string& web_url,
    const ui::AXTreeUpdate& update) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto root = mojom::VoiceInteractionStructure::New();
  root->rect = bounds;
  root->class_name = "android.view.dummy.root.WebUrl";
  root->text = base::UTF8ToUTF16(web_url);
  root->children.push_back(CreateVoiceInteractionStructure(
      *ui::AXSnapshotNodeAndroid::Create(update, false)));
  callback.Run(std::move(root));
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
ArcVoiceInteractionArcHomeService*
ArcVoiceInteractionArcHomeService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVoiceInteractionArcHomeServiceFactory::GetForBrowserContext(
      context);
}

ArcVoiceInteractionArcHomeService::ArcVoiceInteractionArcHomeService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : context_(context), arc_bridge_service_(bridge_service), binding_(this) {
  arc_bridge_service_->voice_interaction_arc_home()->AddObserver(this);
}

ArcVoiceInteractionArcHomeService::~ArcVoiceInteractionArcHomeService() {
  // TODO(hidehiko): Currently, the lifetime of ArcBridgeService and
  // BrowserContextKeyedService is not nested.
  // If ArcServiceManager::Get() returns nullptr, it is already destructed,
  // so do not touch it.
  if (ArcServiceManager::Get())
    arc_bridge_service_->voice_interaction_arc_home()->RemoveObserver(this);
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

void ArcVoiceInteractionArcHomeService::GetVoiceInteractionStructure(
    const GetVoiceInteractionStructureCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* framework_service =
      ArcVoiceInteractionFrameworkService::GetForBrowserContext(context_);
  if (!framework_service->ValidateTimeSinceUserInteraction()) {
    callback.Run(mojom::VoiceInteractionStructure::New());
    return;
  }
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (!browser || !browser->window()->IsActive()) {
    // TODO(muyuanli): retrieve context for apps.
    LOG(ERROR) << "Retrieving context from apps is not implemented.";
    callback.Run(mojom::VoiceInteractionStructure::New());
    return;
  }

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // Do not process incognito tab.
  if (web_contents->GetBrowserContext()->IsOffTheRecord()) {
    callback.Run(mojom::VoiceInteractionStructure::New());
    return;
  }

  web_contents->RequestAXTreeSnapshot(
      base::Bind(&RequestVoiceInteractionStructureCallback, callback,
                 browser->window()->GetBounds(),
                 web_contents->GetLastCommittedURL().spec()));
}

void ArcVoiceInteractionArcHomeService::OnVoiceInteractionOobeSetupComplete() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  arc::ArcPaiStarter* pai_starter =
      arc::ArcSessionManager::Get()->pai_starter();
  if (pai_starter)
    pai_starter->ReleaseLock();
  chromeos::first_run::MaybeLaunchDialogImmediately();
  arc::ArcVoiceInteractionFrameworkService::GetForBrowserContext(context_)
      ->UpdateVoiceInteractionPrefs();
}

// static
mojom::VoiceInteractionStructurePtr
ArcVoiceInteractionArcHomeService::CreateVoiceInteractionStructureForTesting(
    const ui::AXSnapshotNodeAndroid& view_structure) {
  return CreateVoiceInteractionStructure(view_structure);
}

}  // namespace arc
