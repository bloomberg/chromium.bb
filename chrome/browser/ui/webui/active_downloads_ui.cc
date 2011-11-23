// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/active_downloads_ui.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_piece.h"
#include "base/string_util.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/media/media_player.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/fileicon_source_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_file_job.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

static const int kPopupLeft = 0;
static const int kPopupTop = 0;
static const int kPopupWidth = 250;
// Minimum height of window must be 100, so kPopupHeight has space for
// 2 download rows of 36 px and 'Show all files' link which is 29px.
static const int kPopupHeight = 36 * 2 + 29;

static const char kPropertyPath[] = "path";
static const char kPropertyTitle[] = "title";
static const char kPropertyDirectory[] = "isDirectory";
static const char kActiveDownloadAppName[] = "active-downloads";

ChromeWebUIDataSource* CreateActiveDownloadsUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIActiveDownloadsHost);

  source->AddLocalizedString("dangerousfile", IDS_PROMPT_DANGEROUS_DOWNLOAD);
  source->AddLocalizedString("dangerousextension",
                             IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
  source->AddLocalizedString("dangerousurl", IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
  source->AddLocalizedString("dangerouscontent",
                             IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT);
  source->AddLocalizedString("cancel", IDS_DOWNLOAD_LINK_CANCEL);
  source->AddLocalizedString("discard", IDS_DISCARD_DOWNLOAD);
  source->AddLocalizedString("continue", IDS_CONTINUE_EXTENSION_DOWNLOAD);
  source->AddLocalizedString("pause", IDS_DOWNLOAD_LINK_PAUSE);
  source->AddLocalizedString("resume", IDS_DOWNLOAD_LINK_RESUME);
  source->AddLocalizedString("showallfiles",
                             IDS_FILE_BROWSER_MORE_FILES);
  source->AddLocalizedString("error_unknown_file_type",
                             IDS_FILEBROWSER_ERROR_UNKNOWN_FILE_TYPE);

  FilePath default_download_path;
  if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                        &default_download_path)) {
    NOTREACHED();
  }
  // TODO(viettrungluu): this is wrong -- FilePath's need not be Unicode.
  source->AddString("downloadpath", UTF8ToUTF16(default_download_path.value()));

  source->set_json_path("strings.js");
  source->add_resource_path("active_downloads.js", IDR_ACTIVE_DOWNLOADS_JS);
  source->set_default_resource(IDR_ACTIVE_DOWNLOADS_HTML);
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//
// ActiveDownloadsHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages related to the "active_downloads" view.
class ActiveDownloadsHandler
    : public WebUIMessageHandler,
      public DownloadManager::Observer,
      public DownloadItem::Observer {
 public:
  ActiveDownloadsHandler();
  virtual ~ActiveDownloadsHandler();

  // Initialization after Attach.
  void Init();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(DownloadItem* item) OVERRIDE;
  virtual void OnDownloadOpened(DownloadItem* item) OVERRIDE { }

  // DownloadManager::Observer interface.
  virtual void ModelChanged() OVERRIDE;

  // WebUI Callbacks.
  void HandleGetDownloads(const ListValue* args);
  void HandlePauseToggleDownload(const ListValue* args);
  void HandleAllowDownload(const ListValue* args);
  void HandleCancelDownload(const ListValue* args);
  void HandleShowAllFiles(const ListValue* args);
  void OpenNewFullWindow(const ListValue* args);
  void PlayMediaFile(const ListValue* args);

  // For testing.
  typedef std::vector<DownloadItem*> DownloadList;
  const DownloadList& downloads() const { return downloads_; }

 private:
  // Downloads helpers.
  DownloadItem* GetDownloadById(const ListValue* args);
  void UpdateDownloadList();
  void SendDownloads();
  void AddDownload(DownloadItem* item);
  bool SelectTab(const GURL& url);

  Profile* profile_;
  TabContents* tab_contents_;
  DownloadManager* download_manager_;

  DownloadList active_downloads_;
  DownloadList downloads_;

  DISALLOW_COPY_AND_ASSIGN(ActiveDownloadsHandler);
};

ActiveDownloadsHandler::ActiveDownloadsHandler()
    : profile_(NULL),
      tab_contents_(NULL),
      download_manager_(NULL) {
}

ActiveDownloadsHandler::~ActiveDownloadsHandler() {
  for (size_t i = 0; i < downloads_.size(); ++i) {
    downloads_[i]->RemoveObserver(this);
  }
  download_manager_->RemoveObserver(this);
}

WebUIMessageHandler* ActiveDownloadsHandler::Attach(WebUI* web_ui) {
  profile_ = Profile::FromWebUI(web_ui);
  profile_->GetChromeURLDataManager()->AddDataSource(new FileIconSourceCros());
  tab_contents_ = web_ui->tab_contents();
  return WebUIMessageHandler::Attach(web_ui);
}

void ActiveDownloadsHandler::Init() {
  download_manager_ =
      DownloadServiceFactory::GetForProfile(profile_)->GetDownloadManager();
  download_manager_->AddObserver(this);
}

void ActiveDownloadsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getDownloads",
      base::Bind(&ActiveDownloadsHandler::HandleGetDownloads,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("pauseToggleDownload",
      base::Bind(&ActiveDownloadsHandler::HandlePauseToggleDownload,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("allowDownload",
      base::Bind(&ActiveDownloadsHandler::HandleAllowDownload,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("cancelDownload",
      base::Bind(&ActiveDownloadsHandler::HandleCancelDownload,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("showAllFiles",
      base::Bind(&ActiveDownloadsHandler::HandleShowAllFiles,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("openNewFullWindow",
      base::Bind(&ActiveDownloadsHandler::OpenNewFullWindow,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("playMediaFile",
      base::Bind(&ActiveDownloadsHandler::PlayMediaFile,
                 base::Unretained(this)));
}

void ActiveDownloadsHandler::PlayMediaFile(const ListValue* args) {
  FilePath file_path(UTF16ToUTF8(ExtractStringValue(args)));

  Browser* browser = Browser::GetBrowserForController(
      &tab_contents_->controller(), NULL);
  MediaPlayer* mediaplayer = MediaPlayer::GetInstance();
  mediaplayer->PopupMediaPlayer(browser);
  mediaplayer->ForcePlayMediaFile(profile_, file_path);
}

DownloadItem* ActiveDownloadsHandler::GetDownloadById(
    const ListValue* args) {
  int i;
  if (!ExtractIntegerValue(args, &i))
    return NULL;
  size_t id(i);
  return id < downloads_.size() ? downloads_[id] : NULL;
}

void ActiveDownloadsHandler::HandlePauseToggleDownload(const ListValue* args) {
  DownloadItem* item = GetDownloadById(args);
  if (item && item->IsPartialDownload())
    item->TogglePause();
}

void ActiveDownloadsHandler::HandleAllowDownload(const ListValue* args) {
  DownloadItem* item = GetDownloadById(args);
  if (item)
    item->DangerousDownloadValidated();
}

void ActiveDownloadsHandler::HandleCancelDownload(const ListValue* args) {
  DownloadItem* item = GetDownloadById(args);
  if (item) {
    if (item->IsPartialDownload())
      item->Cancel(true);
    item->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
  }
}

void ActiveDownloadsHandler::HandleShowAllFiles(const ListValue* args) {
  file_manager_util::ViewFolder(
      DownloadPrefs::FromDownloadManager(download_manager_)->download_path());
}

bool ActiveDownloadsHandler::SelectTab(const GURL& url) {
  for (TabContentsIterator it; !it.done(); ++it) {
    TabContents* tab_contents = it->tab_contents();
    if (tab_contents->GetURL() == url) {
      static_cast<RenderViewHostDelegate*>(tab_contents)->Activate();
      return true;
    }
  }
  return false;
}

void ActiveDownloadsHandler::OpenNewFullWindow(const ListValue* args) {
  std::string url = UTF16ToUTF8(ExtractStringValue(args));

  if (SelectTab(GURL(url)))
    return;

  DCHECK(profile_);
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  browser::NavigateParams params(
      browser, GURL(url), content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&params);
  browser->window()->Show();
}

void ActiveDownloadsHandler::ModelChanged() {
  UpdateDownloadList();
}

void ActiveDownloadsHandler::HandleGetDownloads(const ListValue* args) {
  UpdateDownloadList();
}

void ActiveDownloadsHandler::UpdateDownloadList() {
  DownloadList downloads;
  download_manager_->GetAllDownloads(FilePath(), &downloads);
  active_downloads_.clear();
  for (size_t i = 0; i < downloads.size(); ++i) {
    AddDownload(downloads[i]);
  }
  SendDownloads();
}

void ActiveDownloadsHandler::AddDownload(DownloadItem* item) {
  // Observe in progress and dangerous downloads.
  if (item->GetState() == DownloadItem::IN_PROGRESS ||
      item->GetSafetyState() == DownloadItem::DANGEROUS) {
    active_downloads_.push_back(item);

    DownloadList::const_iterator it =
      std::find(downloads_.begin(), downloads_.end(), item);
    if (it == downloads_.end()) {
      downloads_.push_back(item);
      item->AddObserver(this);
    }
  }
}

void ActiveDownloadsHandler::SendDownloads() {
  ListValue results;
  for (size_t i = 0; i < downloads_.size(); ++i) {
    results.Append(download_util::CreateDownloadItemValue(downloads_[i], i));
  }

  web_ui_->CallJavascriptFunction("downloadsList", results);
}

void ActiveDownloadsHandler::OnDownloadUpdated(DownloadItem* item) {
  DownloadList::iterator it =
      find(downloads_.begin(), downloads_.end(), item);

  if (it == downloads_.end()) {
    NOTREACHED() << "Updated item " << item->GetFullPath().value()
      << " not found";
  }

  if (item->GetState() == DownloadItem::REMOVING || item->GetAutoOpened()) {
    // Item is going away, or item is an extension that has auto opened.
    item->RemoveObserver(this);
    downloads_.erase(it);
    DownloadList::iterator ita =
        find(active_downloads_.begin(), active_downloads_.end(), item);
    if (ita != active_downloads_.end())
      active_downloads_.erase(ita);
    SendDownloads();
  } else {
    const size_t id = it - downloads_.begin();
    scoped_ptr<DictionaryValue> result(
        download_util::CreateDownloadItemValue(item, id));
    web_ui_->CallJavascriptFunction("downloadUpdated", *result);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
// ActiveDownloadsUI
//
////////////////////////////////////////////////////////////////////////////////


ActiveDownloadsUI::ActiveDownloadsUI(TabContents* contents)
    : HtmlDialogUI(contents),
      handler_(new ActiveDownloadsHandler()) {
  AddMessageHandler(handler_->Attach(this));
  handler_->Init();

  // Set up the chrome://active-downloads/ source.
  Profile* profile = Profile::FromBrowserContext(contents->browser_context());
  profile->GetChromeURLDataManager()->AddDataSource(
      CreateActiveDownloadsUIHTMLSource());
}

#if defined(TOUCH_UI)

// static
TabContents* ActiveDownloadsUI::OpenPopup(Profile* profile) {
  Browser* browser = Browser::GetOrCreateTabbedBrowser(profile);
  OpenURLParams params(GURL(chrome::kChromeUIActiveDownloadsURL), GURL(),
                      SINGLETON_TAB, content::PAGE_TRANSITION_LINK, false);
  TabContents* download_contents = browser->OpenURL(params);
  browser->window()->Show();
  return download_contents;
}

TabContents* ActiveDownloadsUI::GetPopup(Browser** browser) {
  for (TabContentsIterator it; !it.done(); ++it) {
    TabContents* tab = it->tab_contents();
    const GURL& url = tab->GetURL();
    if (url.SchemeIs(chrome::kChromeUIScheme) &&
        url.host() == chrome::kChromeUIActiveDownloadsHost) {
      if (browser)
        *browser = it.browser();
      return tab;
    }
  }
  return NULL;
}

#else  // defined(TOUCH_UI)
// static
Browser* ActiveDownloadsUI::OpenPopup(Profile* profile) {
  Browser* browser = GetPopup();

  // Create new browser if no matching pop up is found.
  if (browser == NULL) {
    browser = Browser::CreateForApp(Browser::TYPE_PANEL, kActiveDownloadAppName,
                                    gfx::Rect(), profile);

    browser::NavigateParams params(
        browser,
        GURL(chrome::kChromeUIActiveDownloadsURL),
        content::PAGE_TRANSITION_LINK);
    params.disposition = NEW_FOREGROUND_TAB;
    browser::Navigate(&params);

    DCHECK_EQ(browser, params.browser);
    // TODO(beng): The following two calls should be automatic by Navigate().
    browser->window()->SetBounds(gfx::Rect(kPopupLeft,
                                           kPopupTop,
                                           kPopupWidth,
                                           kPopupHeight));
  }

  browser->window()->Show();
  return browser;
}

Browser* ActiveDownloadsUI::GetPopup() {
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it) {
    if ((*it)->is_type_panel() && (*it)->is_app()) {
      TabContents* tab_contents = (*it)->GetSelectedTabContents();
      DCHECK(tab_contents);
      if (!tab_contents)
        continue;
      const GURL& url = tab_contents->GetURL();

      if (url.SchemeIs(chrome::kChromeUIScheme) &&
          url.host() == chrome::kChromeUIActiveDownloadsHost) {
        return (*it);
      }
    }
  }
  return NULL;
}
#endif  // defined(TOUCH_UI)

const ActiveDownloadsUI::DownloadList& ActiveDownloadsUI::GetDownloads() const {
  return handler_->downloads();
}
