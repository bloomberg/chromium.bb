// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_ui.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/singleton.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserContext;
using content::DownloadManager;
using content::WebContents;

namespace {

content::WebUIDataSource* CreateDownloadsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDownloadsHost);

  source->AddLocalizedString("title", IDS_DOWNLOAD_TITLE);
  source->AddLocalizedString("searchResultsPlural", IDS_SEARCH_RESULTS_PLURAL);
  source->AddLocalizedString("searchResultsSingular",
                             IDS_SEARCH_RESULTS_SINGULAR);
  source->AddLocalizedString("downloads", IDS_DOWNLOAD_TITLE);

  source->AddLocalizedString("actionMenuDescription",
                             IDS_DOWNLOAD_ACTION_MENU_DESCRIPTION);
  source->AddLocalizedString("clearAll", IDS_DOWNLOAD_LINK_CLEAR_ALL);
  source->AddLocalizedString("clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH);
  source->AddLocalizedString("openDownloadsFolder",
                             IDS_DOWNLOAD_LINK_OPEN_DOWNLOADS_FOLDER);
  source->AddLocalizedString("moreActions", IDS_DOWNLOAD_MORE_ACTIONS);
  source->AddLocalizedString("search", IDS_MD_DOWNLOAD_SEARCH);

  // No results message that shows instead of the downloads list.
  source->AddLocalizedString("noDownloads", IDS_MD_DOWNLOAD_NO_DOWNLOADS);
  source->AddLocalizedString("noSearchResults", IDS_SEARCH_NO_RESULTS);

  // Status.
  source->AddLocalizedString("statusCancelled", IDS_DOWNLOAD_TAB_CANCELLED);
  source->AddLocalizedString("statusRemoved", IDS_DOWNLOAD_FILE_REMOVED);

  // Dangerous file.
  source->AddLocalizedString("dangerFileDesc",
                             IDS_BLOCK_REASON_GENERIC_DOWNLOAD);
  source->AddLocalizedString("dangerDownloadDesc",
                             IDS_BLOCK_REASON_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString("dangerUncommonDesc",
                             IDS_BLOCK_REASON_UNCOMMON_DOWNLOAD);
  source->AddLocalizedString("dangerSettingsDesc",
                             IDS_BLOCK_REASON_UNWANTED_DOWNLOAD);
  source->AddLocalizedString("dangerSave", IDS_CONFIRM_DOWNLOAD);
  source->AddLocalizedString("dangerRestore", IDS_CONFIRM_DOWNLOAD_RESTORE);
  source->AddLocalizedString("dangerDiscard", IDS_DISCARD_DOWNLOAD);

  // Controls.
  source->AddLocalizedString("controlPause", IDS_DOWNLOAD_LINK_PAUSE);
  if (browser_defaults::kDownloadPageHasShowInFolder)
    source->AddLocalizedString("controlShowInFolder", IDS_DOWNLOAD_LINK_SHOW);
  source->AddLocalizedString("controlCancel", IDS_DOWNLOAD_LINK_CANCEL);
  source->AddLocalizedString("controlResume", IDS_DOWNLOAD_LINK_RESUME);
  source->AddLocalizedString("controlRemoveFromList", IDS_DOWNLOAD_LINK_REMOVE);
  source->AddLocalizedString("controlRemoveFromListAriaLabel",
                             IDS_DOWNLOAD_LINK_REMOVE_ARIA_LABEL);
  source->AddLocalizedString("controlRetry", IDS_MD_DOWNLOAD_LINK_RETRY);
  source->AddLocalizedString("controlledByUrl", IDS_DOWNLOAD_BY_EXTENSION_URL);

  PrefService* prefs = profile->GetPrefs();
  source->AddBoolean("allowDeletingHistory",
                     prefs->GetBoolean(prefs::kAllowDeletingBrowserHistory) &&
                         !profile->IsSupervised());

  source->AddLocalizedString("inIncognito", IDS_DOWNLOAD_IN_INCOGNITO);

  source->AddResourcePath("1x/incognito_marker.png",
                          IDR_MD_DOWNLOADS_1X_INCOGNITO_MARKER_PNG);
  source->AddResourcePath("2x/incognito_marker.png",
                          IDR_MD_DOWNLOADS_2X_INCOGNITO_MARKER_PNG);
  source->AddResourcePath("1x/no_downloads.png",
                          IDR_MD_DOWNLOADS_1X_NO_DOWNLOADS_PNG);
  source->AddResourcePath("2x/no_downloads.png",
                          IDR_MD_DOWNLOADS_2X_NO_DOWNLOADS_PNG);
  source->AddResourcePath("downloads.mojom-lite.js",
                          IDR_MD_DOWNLOADS_MOJO_LITE_JS);

#if BUILDFLAG(OPTIMIZE_WEBUI)
  source->UseGzip(base::BindRepeating([](const std::string& path) {
    return path != "1x/incognito_marker.png" && path != "1x/no_downloads.png" &&
           path != "2x/incognito_marker.png" && path != "2x/no_downloads.png" &&
           path != "downloads.mojom-lite.js";
  }));

  source->AddResourcePath("crisper.js", IDR_MD_DOWNLOADS_CRISPER_JS);
  source->SetDefaultResource(
      base::FeatureList::IsEnabled(features::kWebUIPolymer2)
          ? IDR_MD_DOWNLOADS_VULCANIZED_P2_HTML
          : IDR_MD_DOWNLOADS_VULCANIZED_HTML);
#else
  source->AddResourcePath("browser_proxy.html",
                          IDR_MD_DOWNLOADS_BROWSER_PROXY_HTML);
  source->AddResourcePath("browser_proxy.js",
                          IDR_MD_DOWNLOADS_BROWSER_PROXY_JS);
  source->AddResourcePath("constants.html", IDR_MD_DOWNLOADS_CONSTANTS_HTML);
  source->AddResourcePath("constants.js", IDR_MD_DOWNLOADS_CONSTANTS_JS);
  source->AddResourcePath("downloads.js", IDR_MD_DOWNLOADS_DOWNLOADS_JS);
  source->AddResourcePath("i18n_setup.html", IDR_MD_DOWNLOADS_I18N_SETUP_HTML);
  source->AddResourcePath("icon_loader.html",
                          IDR_MD_DOWNLOADS_ICON_LOADER_HTML);
  source->AddResourcePath("icon_loader.js", IDR_MD_DOWNLOADS_ICON_LOADER_JS);
  source->AddResourcePath("icons.html", IDR_MD_DOWNLOADS_ICONS_HTML);
  source->AddResourcePath("item.html", IDR_MD_DOWNLOADS_ITEM_HTML);
  source->AddResourcePath("item.js", IDR_MD_DOWNLOADS_ITEM_JS);
  source->AddResourcePath("manager.html", IDR_MD_DOWNLOADS_MANAGER_HTML);
  source->AddResourcePath("manager.js", IDR_MD_DOWNLOADS_MANAGER_JS);
  source->AddResourcePath("search_service.html",
                          IDR_MD_DOWNLOADS_SEARCH_SERVICE_HTML);
  source->AddResourcePath("search_service.js",
                          IDR_MD_DOWNLOADS_SEARCH_SERVICE_JS);
  source->AddResourcePath("toolbar.html", IDR_MD_DOWNLOADS_TOOLBAR_HTML);
  source->AddResourcePath("toolbar.js", IDR_MD_DOWNLOADS_TOOLBAR_JS);
  source->SetDefaultResource(IDR_MD_DOWNLOADS_DOWNLOADS_HTML);
#endif

  source->SetJsonPath("strings.js");

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// MdDownloadsUI
//
///////////////////////////////////////////////////////////////////////////////

MdDownloadsUI::MdDownloadsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true), page_factory_binding_(this) {
  Profile* profile = Profile::FromWebUI(web_ui);
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

  // Set up the chrome://downloads/ source.
  content::WebUIDataSource* source = CreateDownloadsUIHTMLSource(profile);
  DarkModeHandler::Initialize(web_ui, source);
  ManagedUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(profile, source);
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));

  AddHandlerToRegistry(base::BindRepeating(
      &MdDownloadsUI::BindPageHandlerFactory, base::Unretained(this)));
}

MdDownloadsUI::~MdDownloadsUI() = default;

// static
base::RefCountedMemory* MdDownloadsUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_DOWNLOADS_FAVICON, scale_factor);
}

void MdDownloadsUI::BindPageHandlerFactory(
    md_downloads::mojom::PageHandlerFactoryRequest request) {
  if (page_factory_binding_.is_bound())
    page_factory_binding_.Unbind();

  page_factory_binding_.Bind(std::move(request));
}

void MdDownloadsUI::CreatePageHandler(
    md_downloads::mojom::PagePtr page,
    md_downloads::mojom::PageHandlerRequest request) {
  DCHECK(page);
  Profile* profile = Profile::FromWebUI(web_ui());
  DownloadManager* dlm = BrowserContext::GetDownloadManager(profile);

  page_handler_.reset(new MdDownloadsDOMHandler(
      std::move(request), std::move(page), dlm, web_ui()));
}
