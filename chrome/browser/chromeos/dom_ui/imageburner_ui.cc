// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/imageburner_ui.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/download/download_types.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

static const char kPropertyPath[] = "path";
static const char kPropertyTitle[] = "title";
static const char kPropertyDirectory[] = "isDirectory";
static const char kImageBaseURL[] =
    "http://chrome-master.mtv.corp.google.com/chromeos/dev-channel/";
static const char kImageFetcherName[] = "LATEST-x86-generic";
static const char kImageDownloadURL[] =
    "https://dl.google.com/dl/chromeos/recovery/latest_mario_beta_channel";
static const char kImageFileName[] = "chromeos_image.bin.zip";
static const char kTempImageFolderName[] = "chromeos_image";

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnUIHTMLSource
//
////////////////////////////////////////////////////////////////////////////////

class ImageBurnUIHTMLSource : public ChromeURLDataManager::DataSource {
 public:
  ImageBurnUIHTMLSource()
      : DataSource(chrome::kChromeUIImageBurnerHost, MessageLoop::current()) {
  }

  // Called when the network layer has requested a resource underneath
  // the path we registered.
  virtual void StartDataRequest(const std::string& path,
                                 bool is_off_the_record,
                                 int request_id) {
    DictionaryValue localized_strings;
    localized_strings.SetString("burnConfirmText1",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_CONFIM_BURN1));
    localized_strings.SetString("burnConfirmText2",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_CONFIM_BURN2));
    localized_strings.SetString("burnUnsuccessfulMessage",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_UNSUCCESSFUL));
    localized_strings.SetString("burnSuccessfulMessage",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_SUCCESSFUL));
    localized_strings.SetString("downloadAbortedMessage",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOAD_UNSUCCESSFUL));
    localized_strings.SetString("title",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_TITLE));
    localized_strings.SetString("listTitle",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_ROOT_LIST_TITLE));
    localized_strings.SetString("downloadStatusStart",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOAD_STARTING_STATUS));
    localized_strings.SetString("downloadStatusInProgress",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOAD_IN_PROGRESS_STATUS));
    localized_strings.SetString("downloadStatusComplete",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOAD_COMPLETE_STATUS));
    localized_strings.SetString("downloadStatusCanceled",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOAD_CANCELED_STATUS));
    localized_strings.SetString("burnStatusStart",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_STARTING_STATUS));
    localized_strings.SetString("burnStatusInProgress",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_IN_PROGRESS_STATUS));
    localized_strings.SetString("burnStatusComplete",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_COMPLETE_STATUS));
    localized_strings.SetString("burnStatusCanceled",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_CANCELED_STATUS));

    SetFontAndTextDirection(&localized_strings);

    static const base::StringPiece imageburn_html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_IMAGEBURNER_HTML));
    const std::string full_html = jstemplate_builder::GetI18nTemplateHtml(
        imageburn_html, &localized_strings);

    scoped_refptr<RefCountedBytes> html_bytes(new RefCountedBytes);
    html_bytes->data.resize(full_html.size());
    std::copy(full_html.begin(), full_html.end(), html_bytes->data.begin());

    SendResponse(request_id, html_bytes);
  }

  virtual std::string GetMimeType(const std::string&) const {
    return "text/html";
  }

 private:
  virtual ~ImageBurnUIHTMLSource() {}

  DISALLOW_COPY_AND_ASSIGN(ImageBurnUIHTMLSource);
};

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnTaskProxy
//
////////////////////////////////////////////////////////////////////////////////

class ImageBurnTaskProxy
    : public base::RefCountedThreadSafe<ImageBurnTaskProxy> {
 public:
  explicit ImageBurnTaskProxy(const base::WeakPtr<ImageBurnHandler>& handler)
      : handler_(handler) {
        resource_manager_ = ImageBurnResourceManager::GetInstance();
  }

  bool ReportDownloadInitialized() {
    bool initialized = resource_manager_-> CheckImageDownloadStarted();
    if (!initialized)
      resource_manager_-> ReportImageDownloadStarted();
    return initialized;
  }

  bool CheckDownloadFinished() {
    return resource_manager_->CheckDownloadFinished();
  }

  void BurnImage() {
    if (!resource_manager_->CheckBurnInProgress() && handler_) {
      resource_manager_->SetBurnInProgress(true);
      handler_->BurnImage();
    }
  }

  void FinalizeBurn(bool success) {
    if (handler_) {
      handler_->FinalizeBurn(success);
      resource_manager_->SetBurnInProgress(false);
    }
  }

  void CreateImageUrl(TabContents* tab_contents, ImageBurnHandler* downloader) {
    resource_manager_->CreateImageUrl(tab_contents, downloader);
  }

 private:
  base::WeakPtr<ImageBurnHandler> handler_;
  ImageBurnResourceManager* resource_manager_;

  friend class base::RefCountedThreadSafe<ImageBurnTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnTaskProxy);
};

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnHandler
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnHandler::ImageBurnHandler(TabContents* contents)
    :tab_contents_(contents),
     download_manager_(NULL),
     download_item_observer_added_(false),
     active_download_item_(NULL),
     burn_resource_manager_(NULL) {
  chromeos::MountLibrary* mount_lib =
          chromeos::CrosLibrary::Get()->GetMountLibrary();
  mount_lib->AddObserver(this);
  chromeos::BurnLibrary* burn_lib =
          chromeos::CrosLibrary::Get()->GetBurnLibrary();
  burn_lib->AddObserver(this);
  local_image_file_path_.clear();
  burn_resource_manager_ = ImageBurnResourceManager::GetInstance();
}

ImageBurnHandler::~ImageBurnHandler() {
  chromeos::MountLibrary* mount_lib =
          chromeos::CrosLibrary::Get()->GetMountLibrary();
  mount_lib->RemoveObserver(this);
  chromeos::BurnLibrary* burn_lib =
          chromeos::CrosLibrary::Get()->GetBurnLibrary();
  burn_lib->RemoveObserver(this);
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
  }
  if (download_manager_)
      download_manager_->RemoveObserver(this);
}

WebUIMessageHandler* ImageBurnHandler::Attach(DOMUI* dom_ui) {
  return WebUIMessageHandler::Attach(dom_ui);
}

void ImageBurnHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("getRoots",
      NewCallback(this, &ImageBurnHandler::HandleGetRoots));
  dom_ui_->RegisterMessageCallback("downloadImage",
      NewCallback(this, &ImageBurnHandler::HandleDownloadImage));
  dom_ui_->RegisterMessageCallback("burnImage",
      NewCallback(this, &ImageBurnHandler::HandleBurnImage));
  dom_ui_->RegisterMessageCallback("cancelBurnImage",
      NewCallback(this, &ImageBurnHandler::HandleCancelBurnImage));
}

void ImageBurnHandler::MountChanged(chromeos::MountLibrary* obj,
                                    chromeos::MountEventType evt,
                                    const std::string& path) {
  if ((evt == chromeos::DISK_REMOVED ||
      evt == chromeos::DISK_CHANGED ||
      evt == chromeos::DEVICE_REMOVED)) {
    dom_ui_->CallJavascriptFunction(L"rootsChanged");
  }
}

void ImageBurnHandler::ProgressUpdated(chromeos::BurnLibrary* object,
                                       chromeos::BurnEventType evt,
                                       const ImageBurnStatus& status) {
    UpdateBurnProgress(status.amount_burnt, status.total_size,
                       status.target_path, evt);
    if (evt == chromeos::BURN_COMPLETE) {
      ImageBurnTaskProxy* task = new ImageBurnTaskProxy(AsWeakPtr());
      task->AddRef();
      task->FinalizeBurn(true);
    } else if (evt == chromeos::BURN_CANCELED) {
      ImageBurnTaskProxy* task = new ImageBurnTaskProxy(AsWeakPtr());
      task->AddRef();
      task->FinalizeBurn(false);
    }
}

void ImageBurnHandler::OnDownloadUpdated(DownloadItem* download) {
  if (download->state() != DownloadItem::CANCELLED
      && download->state() != DownloadItem::COMPLETE) {
    scoped_ptr<DictionaryValue> result_value(
        download_util::CreateDownloadItemValue(download, 0));
    dom_ui_->CallJavascriptFunction(L"downloadUpdated", *result_value);
  }
  if (download->state() == DownloadItem::CANCELLED)
    DownloadCompleted(false);
}

void ImageBurnHandler::OnDownloadFileCompleted(DownloadItem* download) {
  DCHECK(download->state() == DownloadItem::COMPLETE);
  local_image_file_path_ = download->full_path();
  DownloadCompleted(true);
}

void ImageBurnHandler::OnDownloadOpened(DownloadItem* download) {
  if (download->safety_state() == DownloadItem::DANGEROUS)
    download->DangerousDownloadValidated();
}

void ImageBurnHandler::ModelChanged() {
  // Find our item and observe it.
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(
      burn_resource_manager_->GetLocalImageDirPath(), &downloads);
  if (download_item_observer_added_)  // Already added.
    return;
  std::vector<DownloadItem*>::const_iterator it = downloads.begin();
  for (; it != downloads.end(); ++it) {
    if ((*it)->original_url() == *image_download_url_) {
      // Found it.
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void ImageBurnHandler::HandleGetRoots(const ListValue* args) {
  ListValue results_value;
  DictionaryValue info_value;
  chromeos::MountLibrary* mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  const chromeos::MountLibrary::DiskVector& disks = mount_lib->disks();
  if (!burn_resource_manager_->CheckBurnInProgress()) {
    for (size_t i = 0; i < disks.size(); ++i) {
      if (!disks[i].mount_path.empty()) {
        FilePath disk_path = FilePath(disks[i].system_path).DirName();
        std::string title = "/dev/" + disk_path.BaseName().value();
        if (!mount_lib->IsBootPath(title.c_str())) {
          DictionaryValue* page_value = new DictionaryValue();
          page_value->SetString(std::string(kPropertyTitle), title);
          page_value->SetString(std::string(kPropertyPath), title);
          page_value->SetBoolean(std::string(kPropertyDirectory), true);
          results_value.Append(page_value);
        }
      }
    }
  }
  info_value.SetString("functionCall", "getRoots");
  info_value.SetString(std::string(kPropertyPath), "");
  dom_ui_->CallJavascriptFunction(L"browseFileResult",
                                  info_value, results_value);
}

void ImageBurnHandler::HandleDownloadImage(const ListValue* args) {
  ExtractTargetedDeviceSystemPath(args);
  CreateLocalImagePath();
  CreateImageUrl();
}

void ImageBurnHandler::HandleBurnImage(const ListValue* args) {
  ImageBurnTaskProxy* task = new ImageBurnTaskProxy(AsWeakPtr());
  task->AddRef();
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(task, &ImageBurnTaskProxy::BurnImage));
}

void ImageBurnHandler::HandleCancelBurnImage(const ListValue* args) {
  image_target_.clear();
}

void ImageBurnHandler::DownloadCompleted(bool is_successful) {
  burn_resource_manager_-> ReportDownloadFinished(is_successful);
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;

  DictionaryValue signal_value;
  if (is_successful) {
    signal_value.SetString("state", "COMPLETE");
    dom_ui_->CallJavascriptFunction(L"downloadUpdated", signal_value);
    dom_ui_->CallJavascriptFunction(L"promtUserDownloadFinished");
  } else {
    signal_value.SetString("state", "CANCELLED");
    dom_ui_->CallJavascriptFunction(L"downloadUpdated", signal_value);
    dom_ui_->CallJavascriptFunction(L"alertUserDownloadAborted");
  }
}

void ImageBurnHandler::BurnImage() {
  chromeos::BurnLibrary* burn_lib =
          chromeos::CrosLibrary::Get()->GetBurnLibrary();
  if (burn_lib->DoBurn(local_image_file_path_, image_target_)) {
    DictionaryValue signal_value;
    signal_value.SetString("state", "IN_PROGRESS");
    signal_value.SetString("path", image_target_.value());
    signal_value.SetInteger("received", 0);
    signal_value.SetString("progress_status_text", "");
    dom_ui_->CallJavascriptFunction(L"burnProgressUpdated", signal_value);
  }
}

void ImageBurnHandler::FinalizeBurn(bool successful) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dom_ui_->CallJavascriptFunction(successful ?
      L"burnSuccessful" : L"burnUnsuccessful");
}

void ImageBurnHandler::UpdateBurnProgress(int64 total_burnt,
                                          int64 image_size,
                                          const std::string& path,
                                          chromeos::BurnEventType event) {
  DictionaryValue progress_value;
  progress_value.SetString("progress_status_text",
      GetBurnProgressText(total_burnt, image_size));
  if (event == chromeos::BURN_UPDATED)
    progress_value.SetString("state", "IN_PROGRESS");
  else if (event == chromeos::BURN_CANCELED)
    progress_value.SetString("state", "CANCELLED");
  else if (event == chromeos::BURN_COMPLETE)
    progress_value.SetString("state", "COMPLETE");
  progress_value.SetInteger("received", total_burnt);
  progress_value.SetInteger("total", image_size);
  progress_value.SetString("path", path);

  dom_ui_->CallJavascriptFunction(L"burnProgressUpdated", progress_value);
}

string16 ImageBurnHandler::GetBurnProgressText(int64 total_burnt,
                                               int64 image_size) {
  DataUnits amount_units = GetByteDisplayUnits(total_burnt);
  string16 burnt_size = FormatBytes(total_burnt, amount_units, true);

  base::i18n::AdjustStringForLocaleDirection(&burnt_size);

  if (image_size) {
    amount_units = GetByteDisplayUnits(image_size);
    string16 total_text = FormatBytes(image_size, amount_units, true);
    base::i18n::AdjustStringForLocaleDirection(&total_text);

    return l10n_util::GetStringFUTF16(IDS_IMAGEBURN_BURN_PROGRESS,
                                      burnt_size,
                                      total_text);
  } else {
    return l10n_util::GetStringFUTF16(IDS_IMAGEBURN_BURN_PROGRESS_SIZE_UNKNOWN,
                                      burnt_size);
  }
}

void ImageBurnHandler::CreateImageUrl() {
  ImageBurnTaskProxy* task = new ImageBurnTaskProxy(AsWeakPtr());
  task->AddRef();
  task->CreateImageUrl(tab_contents_, this);
}

void ImageBurnHandler::CreateImageUrlCallback(GURL* image_url) {
  if (!image_url) {
    DownloadCompleted(false);
    return;
  }
  image_download_url_ = image_url;

  download_manager_ = tab_contents_->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);

  ImageBurnTaskProxy* task = new ImageBurnTaskProxy(AsWeakPtr());
  task->AddRef();

  if (!task->ReportDownloadInitialized()) {
    DownloadSaveInfo save_info;
    save_info.file_path = local_image_file_path_;
    net::FileStream* file_stream =
        burn_resource_manager_->CreateFileStream(&local_image_file_path_);
    if (!file_stream) {
      DownloadCompleted(false);
      return;
    }
    save_info.file_stream = linked_ptr<net::FileStream>(file_stream);

    download_manager_->DownloadUrlToFile(*image_download_url_,
                                         tab_contents_->GetURL(),
                                         tab_contents_->encoding(),
                                         save_info,
                                         tab_contents_);
  } else if (task->CheckDownloadFinished()) {
    DownloadCompleted(true);
  }
}

void ImageBurnHandler::ExtractTargetedDeviceSystemPath(
    const ListValue* list_value) {
  Value* list_member;
  std::string image_dest;
  if (list_value->Get(0, &list_member) &&
      list_member->GetType() == Value::TYPE_STRING) {
    const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
    string_value->GetAsString(&image_dest);
  } else {
    LOG(ERROR) << "Unable to get path string";
    return;
  }
  image_target_ = FilePath(image_dest);
}

void ImageBurnHandler::CreateLocalImagePath() {
  local_image_file_path_ =
      burn_resource_manager_->GetLocalImageDirPath().Append(kImageFileName);
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnResourceManager
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnResourceManager::ImageBurnResourceManager()
    : image_download_started_(false),
      image_download_finished_(false),
      burn_in_progress_(false),
      download_manager_(NULL),
      download_item_observer_added_(false),
      active_download_item_(NULL),
      image_url_(new GURL(kImageDownloadURL)),
      image_fetcher_url_(std::string(kImageBaseURL) + kImageFetcherName),
      image_url_fetching_requested_(false),
      image_url_fetched_(true) {
  local_image_dir_file_path_.clear();
  image_fecher_local_path_ = GetLocalImageDirPath().Append(kImageFetcherName);
}

ImageBurnResourceManager::~ImageBurnResourceManager() {
  if (!local_image_dir_file_path_.empty()) {
    file_util::Delete(local_image_dir_file_path_, true);
  }
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
  }
  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

// static
ImageBurnResourceManager* ImageBurnResourceManager::GetInstance() {
  return Singleton<ImageBurnResourceManager>::get();
}

void ImageBurnResourceManager::OnDownloadUpdated(DownloadItem* download) {
  if (download->state() == DownloadItem::CANCELLED) {
    image_url_.reset();
    ImageUrlFetched(false);
  }
}

void ImageBurnResourceManager::OnDownloadFileCompleted(DownloadItem* download) {
  DCHECK(download->state() == DownloadItem::COMPLETE);
  std::string image_url;
  if (file_util::ReadFileToString(image_fecher_local_path_, &image_url)) {
    image_url_.reset(new GURL(std::string(kImageBaseURL) + image_url));
    ImageUrlFetched(true);
  } else {
    image_url_.reset();
    ImageUrlFetched(false);
  }
}

void ImageBurnResourceManager::OnDownloadOpened(DownloadItem* download) { }

void ImageBurnResourceManager::ModelChanged() {
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(GetLocalImageDirPath(), &downloads);

  if (download_item_observer_added_)
    return;
  std::vector<DownloadItem*>::const_iterator it = downloads.begin();
  for (; it != downloads.end(); ++it) {
    if ((*it)->url() == image_fetcher_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
    }
  }
}

FilePath ImageBurnResourceManager::GetLocalImageDirPath() {
  if (local_image_dir_file_path_.empty()) {
    CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS,
                           &local_image_dir_file_path_));
    local_image_dir_file_path_ =
        local_image_dir_file_path_.Append(kTempImageFolderName);
    file_util::CreateDirectory(local_image_dir_file_path_);
  }
  return local_image_dir_file_path_;
}

bool ImageBurnResourceManager::CheckImageDownloadStarted() {
  return image_download_started_;
}

void ImageBurnResourceManager::ReportImageDownloadStarted() {
  image_download_started_ = true;
}

bool ImageBurnResourceManager::CheckDownloadFinished() {
  return image_download_finished_;
}

bool ImageBurnResourceManager::CheckBurnInProgress() {
  return burn_in_progress_;
}

void ImageBurnResourceManager::SetBurnInProgress(bool value) {
  burn_in_progress_ = value;
}

void ImageBurnResourceManager::ReportDownloadFinished(bool success) {
  if (!image_download_started_)
    return;
  if (!success)
    image_download_started_ = false;
  image_download_finished_ = success;
}

void ImageBurnResourceManager::CreateImageUrl(TabContents* tab_contents,
                                              ImageBurnHandler* downloader) {
  if (image_url_fetched_) {
    downloader->CreateImageUrlCallback(image_url_.get());
    return;
  }
  downloaders_.push_back(downloader);

  if (image_url_fetching_requested_) {
    return;
  }
  image_url_fetching_requested_ = true;

  download_manager_ = tab_contents->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);

  DownloadSaveInfo save_info;
  save_info.file_path = image_fecher_local_path_;
  net::FileStream* file_stream = CreateFileStream(&image_fecher_local_path_);
  if (!file_stream) {
    ImageUrlFetched(false);
    return;
  }
  save_info.file_stream = linked_ptr<net::FileStream>(file_stream);
  download_manager_->DownloadUrlToFile(image_fetcher_url_,
                                       tab_contents->GetURL(),
                                       tab_contents->encoding(),
                                       save_info,
                                       tab_contents);
}

void ImageBurnResourceManager::ImageUrlFetched(bool success) {
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_)
    download_manager_->RemoveObserver(this);
  if (!success)
    image_url_fetching_requested_ = false;
  image_url_fetched_ = success;
  for (size_t i = 0; i < downloaders_.size(); ++i)
    downloaders_[i]->CreateImageUrlCallback(image_url_.get());
  downloaders_.clear();
}

net::FileStream* ImageBurnResourceManager::CreateFileStream(
                                               FilePath* file_path) {
  DCHECK(file_path && !file_path->empty());
  scoped_ptr<net::FileStream> file_stream(new net::FileStream);
  if (file_stream->Open(*file_path, base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE)) {
    return NULL;
  }
  return file_stream.release();
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnUI
//
////////////////////////////////////////////////////////////////////////////////
ImageBurnUI::ImageBurnUI(TabContents* contents) : DOMUI(contents) {
  ImageBurnHandler* handler = new ImageBurnHandler(contents);
  AddMessageHandler((handler)->Attach(this));
  ImageBurnUIHTMLSource* html_source = new ImageBurnUIHTMLSource();
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
