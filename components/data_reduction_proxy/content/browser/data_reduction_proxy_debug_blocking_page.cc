// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/data_reduction_proxy_debug_blocking_page.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "ui/base/webui/web_ui_util.h"

namespace data_reduction_proxy {

namespace {
// The commands returned by the page when the user performs an action.
const char kProceedCommand[] = "proceed";
const char kTakeMeBackCommand[] = "takeMeBack";

base::LazyInstance<DataReductionProxyDebugBlockingPage::BypassResourceMap>
    g_bypass_resource_map = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
DataReductionProxyDebugBlockingPageFactory*
DataReductionProxyDebugBlockingPage::factory_ = NULL;

// The default DataReductionProxyDebugBlockingPageFactory. Global, made a
// singleton so it isn't leaked.
class DataReductionProxyDebugBlockingPageFactoryImpl
    : public DataReductionProxyDebugBlockingPageFactory {
 public:
  DataReductionProxyDebugBlockingPage* CreateDataReductionProxyPage(
      DataReductionProxyDebugUIManager* ui_manager,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      content::WebContents* web_contents,
      const DataReductionProxyDebugBlockingPage::BypassResourceList&
          resource_list,
      const std::string& app_locale) override {
    return new DataReductionProxyDebugBlockingPage(
        ui_manager, io_task_runner, web_contents, resource_list, app_locale);
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<
      DataReductionProxyDebugBlockingPageFactoryImpl>;

  DataReductionProxyDebugBlockingPageFactoryImpl() {
  }

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyDebugBlockingPageFactoryImpl);
};

static base::LazyInstance<DataReductionProxyDebugBlockingPageFactoryImpl>
    g_data_reduction_proxy_blocking_page_factory_impl =
        LAZY_INSTANCE_INITIALIZER;

// static
content::InterstitialPageDelegate::TypeID
    DataReductionProxyDebugBlockingPage::kTypeForTesting =
        &DataReductionProxyDebugBlockingPage::kTypeForTesting;

DataReductionProxyDebugBlockingPage::DataReductionProxyDebugBlockingPage(
    DataReductionProxyDebugUIManager* ui_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    content::WebContents* web_contents,
    const BypassResourceList& resource_list,
    const std::string& app_locale)
    : ui_manager_(ui_manager),
      io_task_runner_(io_task_runner),
      is_main_frame_load_blocked_(IsMainPageLoadBlocked(resource_list)),
      resource_list_(resource_list),
      proceeded_(false),
      web_contents_(web_contents),
      url_(resource_list[0].url),
      interstitial_page_(NULL),
      create_view_(true),
      app_locale_(app_locale) {
  if (!is_main_frame_load_blocked_) {
    navigation_entry_index_to_remove_ =
        web_contents->GetController().GetLastCommittedEntryIndex();
  } else {
    navigation_entry_index_to_remove_ = -1;
  }
  // Creating interstitial_page_ without showing it leaks memory, so don't
  // create it here.
}

DataReductionProxyDebugBlockingPage::~DataReductionProxyDebugBlockingPage() {
}

content::InterstitialPage*
DataReductionProxyDebugBlockingPage::interstitial_page() const {
  return interstitial_page_;
}

void DataReductionProxyDebugBlockingPage::CommandReceived(
    const std::string& cmd) {
  // Make a local copy so it can be modified.
  std::string command(cmd);
  // The Jasonified response has quotes, remove them.
  if (command.length() > 1 && command[0] == '"') {
    command = command.substr(1, command.length() - 2);
  }

  if (command == kProceedCommand) {
    interstitial_page_->Proceed();
    // |this| has been deleted after Proceed() returns.
    return;
  }

  if (command == kTakeMeBackCommand) {
    if (is_main_frame_load_blocked_) {
      // If the load is blocked, close the interstitial and discard the pending
      // entry.
      interstitial_page_->DontProceed();
      // |this| has been deleted after DontProceed() returns.
      return;
    }

    // Otherwise the offending entry has committed, so go back or to a new page.
    // Close the interstitial when that page commits.
    if (web_contents_->GetController().CanGoBack()) {
      web_contents_->GetController().GoBack();
    } else {
      web_contents_->GetController().LoadURL(
          GURL("chrome://newtab/"),
          content::Referrer(),
          ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
          std::string());
    }
    return;
  }
}

content::InterstitialPageDelegate::TypeID
DataReductionProxyDebugBlockingPage::GetTypeForTesting() const {
  return DataReductionProxyDebugBlockingPage::kTypeForTesting;
}

void DataReductionProxyDebugBlockingPage::OnProceed() {
  proceeded_ = true;

  NotifyDataReductionProxyDebugUIManager(ui_manager_, io_task_runner_,
                                         resource_list_, true);

  // The user is proceeding, an interstitial will not be shown again for a
  // predetermined amount of time. Clear the queued resource notifications
  // received while the interstitial was showing.
  BypassResourceMap* resource_map = GetBypassResourcesMap();
  BypassResourceMap::iterator iter = resource_map->find(web_contents_);
  if (iter != resource_map->end() && !iter->second.empty()) {
    NotifyDataReductionProxyDebugUIManager(ui_manager_, io_task_runner_,
                                           iter->second, true);
    resource_map->erase(iter);
  }
}

void DataReductionProxyDebugBlockingPage::Show() {
  DCHECK(!interstitial_page_);
  interstitial_page_ = content::InterstitialPage::Create(
      web_contents_, is_main_frame_load_blocked_, url_, this);
  if (!create_view_)
    interstitial_page_->DontCreateViewForTesting();
  interstitial_page_->Show();
}

void DataReductionProxyDebugBlockingPage::OnDontProceed() {
  // Proceed() could have already been called, in which case do not notify the
  // DataReductionProxyDebugUIManager again, as the client has been deleted.
  if (proceeded_)
    return;

  NotifyDataReductionProxyDebugUIManager(ui_manager_, io_task_runner_,
                                         resource_list_, false);

  // The user does not want to proceed, clear the queued resource notifications
  // received while the interstitial was showing.
  BypassResourceMap* resource_map = GetBypassResourcesMap();
  BypassResourceMap::iterator iter = resource_map->find(web_contents_);
  if (iter != resource_map->end() && !iter->second.empty()) {
    NotifyDataReductionProxyDebugUIManager(ui_manager_, io_task_runner_,
                                           iter->second, false);
    resource_map->erase(iter);
  }

  // Don't remove the navigation entry if the tab is being destroyed as this
  // would trigger a navigation that would cause trouble as the render view host
  // for the tab has by then already been destroyed. Also, don't delete the
  // current entry if it has been committed again, which is possible on a page
  // that had a subresource warning.
  int last_committed_index =
      web_contents_->GetController().GetLastCommittedEntryIndex();
  if (navigation_entry_index_to_remove_ != -1 &&
      navigation_entry_index_to_remove_ != last_committed_index &&
      !web_contents_->IsBeingDestroyed()) {
    CHECK(web_contents_->GetController().RemoveEntryAtIndex(
        navigation_entry_index_to_remove_));
    navigation_entry_index_to_remove_ = -1;
  }
}

// static
void DataReductionProxyDebugBlockingPage::
NotifyDataReductionProxyDebugUIManager(
    DataReductionProxyDebugUIManager* ui_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const BypassResourceList& resource_list,
    bool proceed) {
  io_task_runner->PostTask(
      FROM_HERE,
      base::Bind(&DataReductionProxyDebugUIManager::OnBlockingPageDone,
                 ui_manager, resource_list, proceed));
}

// static
DataReductionProxyDebugBlockingPage::BypassResourceMap*
    DataReductionProxyDebugBlockingPage::GetBypassResourcesMap() {
  return g_bypass_resource_map.Pointer();
}

void DataReductionProxyDebugBlockingPage::DontCreateViewForTesting() {
  create_view_ = false;
}

// static
DataReductionProxyDebugBlockingPage*
DataReductionProxyDebugBlockingPage::CreateBlockingPage(
    DataReductionProxyDebugUIManager* ui_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    content::WebContents* web_contents,
    const BypassResource& bypass_resource,
    const std::string& app_locale) {
  std::vector<BypassResource> resources;
  resources.push_back(bypass_resource);
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_data_reduction_proxy_blocking_page_factory_impl.Pointer();
  return factory_->CreateDataReductionProxyPage(
      ui_manager, io_task_runner, web_contents, resources, app_locale);
}

// static
void DataReductionProxyDebugBlockingPage::ShowBlockingPage(
    DataReductionProxyDebugUIManager* ui_manager,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const BypassResource& bypass_resource,
    const std::string& app_locale) {
  content::RenderViewHost* render_view_host =
      content:: RenderViewHost::FromID(bypass_resource.render_process_host_id,
                                       bypass_resource.render_view_id);
  content::WebContents* web_contents = nullptr;
  if (render_view_host)
    web_contents = content::WebContents::FromRenderViewHost(render_view_host);

  content::InterstitialPage* interstitial =
      content::InterstitialPage::GetInterstitialPage(web_contents);
  if (interstitial && !bypass_resource.is_subresource) {
    // There is already an interstitial showing and a new one for the main frame
    // is about to be displayed. Just hide the current one, it is now
    // irrelevent.
    interstitial->DontProceed();
    interstitial = NULL;
  }

  if (!interstitial) {
    // There is no interstitial currently showing in that tab, go ahead and
    // show this interstitial.
    DataReductionProxyDebugBlockingPage* blocking_page =
        CreateBlockingPage(ui_manager, io_task_runner, web_contents,
                           bypass_resource, app_locale);
    blocking_page->Show();
    return;
  }

  // This is an interstitial for a page's resource, let's queue it.
  BypassResourceMap* bypassed_resource_map = GetBypassResourcesMap();
  (*bypassed_resource_map)[web_contents].push_back(bypass_resource);
}

// static
bool DataReductionProxyDebugBlockingPage::IsMainPageLoadBlocked(
    const BypassResourceList& resource_list) {
  return resource_list.size() == 1 && !resource_list[0].is_subresource;
}

std::string DataReductionProxyDebugBlockingPage::GetHTMLContents() {
  DCHECK(!resource_list_.empty());

  base::DictionaryValue load_time_data;
  webui::SetLoadTimeDataDefaults(app_locale_, &load_time_data);
  load_time_data.SetString(
      "tabTitle", l10n_util::GetStringUTF16(IDS_DATA_REDUCTION_PROXY_TITLE));
  load_time_data.SetString(
      "primaryButtonText",
      l10n_util::GetStringUTF16(IDS_DATA_REDUCTION_PROXY_BACK_BUTTON));
  load_time_data.SetString(
      "secondaryButtonText",
      l10n_util::GetStringUTF16(IDS_DATA_REDUCTION_PROXY_CONTINUE_BUTTON));
  load_time_data.SetString(
      "heading", l10n_util::GetStringUTF16(
          IDS_DATA_REDUCTION_PROXY_CANNOT_PROXY_HEADING));
  load_time_data.SetString(
      "primaryParagraph",
      l10n_util::GetStringFUTF16(
          IDS_DATA_REDUCTION_PROXY_CANNOT_PROXY_PRIMARY_PARAGRAPH,
          base::UTF8ToUTF16(url_.host())));
  load_time_data.SetString(
      "secondaryParagraph",
      l10n_util::GetStringUTF16(
          IDS_DATA_REDUCTION_PROXY_CANNOT_PROXY_SECONDARY_PARAGRAPH));

  std::string html =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DATA_REDUCTION_PROXY_INTERSTITIAL_HTML).as_string();
  return webui::GetI18nTemplateHtml(html, &load_time_data);
}

}  // namespace data_reduction_proxy
