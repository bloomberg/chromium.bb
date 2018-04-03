// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/discard_reason.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

resource_coordinator::DiscardReason GetDiscardReason(bool urgent) {
  return urgent ? resource_coordinator::DiscardReason::kUrgent
                : resource_coordinator::DiscardReason::kProactive;
}

class DiscardsDetailsProviderImpl : public mojom::DiscardsDetailsProvider {
 public:
  // This instance is deleted when the supplied pipe is destroyed.
  DiscardsDetailsProviderImpl(
      mojo::InterfaceRequest<mojom::DiscardsDetailsProvider> request)
      : binding_(this, std::move(request)) {}

  ~DiscardsDetailsProviderImpl() override {}

  // mojom::DiscardsDetailsProvider overrides:
  void GetTabDiscardsInfo(GetTabDiscardsInfoCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    const resource_coordinator::LifecycleUnitVector lifecycle_units =
        tab_manager->GetSortedLifecycleUnits();

    std::vector<mojom::TabDiscardsInfoPtr> infos;
    infos.reserve(lifecycle_units.size());

    const base::TimeTicks now = resource_coordinator::NowTicks();

    // Convert the LifecycleUnits to a vector of TabDiscardsInfos.
    size_t rank = 1;
    for (auto* lifecycle_unit : lifecycle_units) {
      mojom::TabDiscardsInfoPtr info(mojom::TabDiscardsInfo::New());

      resource_coordinator::TabLifecycleUnitExternal*
          tab_lifecycle_unit_external =
              lifecycle_unit->AsTabLifecycleUnitExternal();
      content::WebContents* contents =
          tab_lifecycle_unit_external->GetWebContents();

      info->tab_url = contents->GetLastCommittedURL().spec();
      // This can be empty for pages without a favicon. The WebUI takes care of
      // showing the chrome://favicon default in that case.
      info->favicon_url = lifecycle_unit->GetIconURL();
      info->title = base::UTF16ToUTF8(lifecycle_unit->GetTitle());
      info->is_media = tab_lifecycle_unit_external->IsMediaTab();
      info->is_discarded = tab_lifecycle_unit_external->IsDiscarded();
      info->discard_count = tab_lifecycle_unit_external->GetDiscardCount();
      info->utility_rank = rank++;
      const base::TimeTicks last_focused_time =
          lifecycle_unit->GetSortKey().last_focused_time;
      const base::TimeDelta elapsed =
          (last_focused_time == base::TimeTicks::Max())
              ? base::TimeDelta()
              : (now - last_focused_time);
      info->last_active_seconds = static_cast<int32_t>(elapsed.InSeconds());
      info->is_auto_discardable =
          tab_lifecycle_unit_external->IsAutoDiscardable();
      info->id = lifecycle_unit->GetID();

      infos.push_back(std::move(info));
    }

    std::move(callback).Run(std::move(infos));
  }

  void SetAutoDiscardable(int32_t tab_id,
                          bool is_auto_discardable,
                          SetAutoDiscardableCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->SetTabAutoDiscardableState(tab_id, is_auto_discardable);
    std::move(callback).Run();
  }

  void DiscardById(int32_t tab_id,
                   bool urgent,
                   DiscardByIdCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->DiscardTabById(tab_id, GetDiscardReason(urgent));
    std::move(callback).Run();
  }

  void FreezeById(int32_t tab_id) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->FreezeTabById(tab_id);
  }

  void Discard(bool urgent, DiscardCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->DiscardTab(GetDiscardReason(urgent));
    std::move(callback).Run();
  }

 private:
  mojo::Binding<mojom::DiscardsDetailsProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(DiscardsDetailsProviderImpl);
};

}  // namespace

DiscardsUI::DiscardsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController<mojom::DiscardsDetailsProvider>(web_ui) {
  std::unique_ptr<content::WebUIDataSource> source(
      content::WebUIDataSource::Create(chrome::kChromeUIDiscardsHost));

  source->AddResourcePath("discards.css", IDR_ABOUT_DISCARDS_CSS);
  source->AddResourcePath("discards.js", IDR_ABOUT_DISCARDS_JS);
  // Full paths (relative to src) are important for Mojom generated files.
  source->AddResourcePath("chrome/browser/ui/webui/discards/discards.mojom.js",
                          IDR_ABOUT_DISCARDS_MOJO_JS);
  source->SetDefaultResource(IDR_ABOUT_DISCARDS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source.release());
}

DiscardsUI::~DiscardsUI() {}

void DiscardsUI::BindUIHandler(mojom::DiscardsDetailsProviderRequest request) {
  ui_handler_ =
      std::make_unique<DiscardsDetailsProviderImpl>(std::move(request));
}
