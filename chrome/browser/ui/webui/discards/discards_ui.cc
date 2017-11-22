// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/discards/discards_ui.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/discard_condition.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_stats.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

namespace {

resource_coordinator::DiscardCondition GetDiscardTabCondition(bool urgent) {
  return urgent ? resource_coordinator::DiscardCondition::kUrgent
                : resource_coordinator::DiscardCondition::kProactive;
}

}  // namespace

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
    resource_coordinator::TabStatsList stats = tab_manager->GetTabStats();

    std::vector<mojom::TabDiscardsInfoPtr> infos;
    infos.reserve(stats.size());

    base::TimeTicks now = resource_coordinator::NowTicks();

    // Convert the TabStatsList to a vector of TabDiscardsInfos.
    size_t rank = 1;
    for (const auto& tab : stats) {
      mojom::TabDiscardsInfoPtr info(mojom::TabDiscardsInfo::New());

      info->tab_url = tab.tab_url;
      // This can be empty for pages without a favicon. The WebUI takes care of
      // showing the chrome://favicon default in that case.
      info->favicon_url = tab.favicon_url;
      info->title = base::UTF16ToUTF8(tab.title);
      info->is_app = tab.is_app;
      info->is_internal = tab.is_internal_page;
      info->is_media = tab.is_media;
      info->is_pinned = tab.is_pinned;
      info->is_discarded = tab.is_discarded;
      info->discard_count = tab.discard_count;
      info->utility_rank = rank++;
      auto elapsed = now - tab.last_active;
      info->last_active_seconds = static_cast<int32_t>(elapsed.InSeconds());
      info->is_auto_discardable = tab.is_auto_discardable;
      info->id = tab.id;

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
    tab_manager->DiscardTabById(tab_id, GetDiscardTabCondition(urgent));
    std::move(callback).Run();
  }

  void Discard(bool urgent, DiscardCallback callback) override {
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    tab_manager->DiscardTab(GetDiscardTabCondition(urgent));
    std::move(callback).Run();
  }

 private:
  mojo::Binding<mojom::DiscardsDetailsProvider> binding_;

  DISALLOW_COPY_AND_ASSIGN(DiscardsDetailsProviderImpl);
};

}  // namespace

DiscardsUI::DiscardsUI(content::WebUI* web_ui)
    : MojoWebUIController<mojom::DiscardsDetailsProvider>(web_ui) {
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
  ui_handler_.reset(new DiscardsDetailsProviderImpl(std::move(request)));
}
