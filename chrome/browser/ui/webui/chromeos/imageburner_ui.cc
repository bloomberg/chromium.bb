// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/imageburner_ui.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_types.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/zip.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
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
                                bool is_incognito,
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
      : handler_(handler) {}

  void CreateImageDir() {
    if (handler_)
      handler_->CreateImageDirOnFileThread(this);
  }

  void OnImageDirCreated(bool success) {
    if (handler_)
      handler_->OnImageDirCreatedOnUIThread(success);
  }

  void BurnImage() {
    if (handler_)
      handler_->BurnImageOnFileThread();
    DeleteOnUIThread();
  }

  void UnzipImage() {
    if (handler_)
      handler_->UnzipImageOnFileThread(this);
  }

  void UnzipComplete(bool success) {
    if (handler_)
      handler_->UnzipComplete(success);
  }

  // ImageBurnTaskProxy is created on the UI thread, so in some cases,
  // we need to post back to the UI thread for destruction.
  void DeleteOnUIThread() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ImageBurnTaskProxy::DoNothing));
  }

  void DoNothing() {}

 private:
  base::WeakPtr<ImageBurnHandler> handler_;

  friend class base::RefCountedThreadSafe<ImageBurnTaskProxy>;
  ~ImageBurnTaskProxy() {}

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
     resource_manager_(NULL) {
  chromeos::CrosLibrary::Get()->GetMountLibrary()->AddObserver(this);
  chromeos::CrosLibrary::Get()->GetBurnLibrary()->AddObserver(this);
  resource_manager_ = ImageBurnResourceManager::GetInstance();
  zip_image_file_path_.clear();
  image_file_path_.clear();
  image_target_.clear();
}

ImageBurnHandler::~ImageBurnHandler() {
  chromeos::CrosLibrary::Get()->GetMountLibrary()->RemoveObserver(this);
  chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
  if (active_download_item_)
    active_download_item_->RemoveObserver(this);
  if (download_manager_)
      download_manager_->RemoveObserver(this);
}

WebUIMessageHandler* ImageBurnHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void ImageBurnHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getRoots",
      NewCallback(this, &ImageBurnHandler::HandleGetRoots));
  web_ui_->RegisterMessageCallback("downloadImage",
      NewCallback(this, &ImageBurnHandler::HandleDownloadImage));
  web_ui_->RegisterMessageCallback("burnImage",
      NewCallback(this, &ImageBurnHandler::HandleBurnImage));
  web_ui_->RegisterMessageCallback("cancelBurnImage",
      NewCallback(this, &ImageBurnHandler::HandleCancelBurnImage));
}

void ImageBurnHandler::DiskChanged(chromeos::MountLibraryEventType event,
                                   const chromeos::MountLibrary::Disk* disk) {
  if (event == chromeos::MOUNT_DISK_REMOVED ||
      event == chromeos::MOUNT_DISK_CHANGED ||
      event == chromeos::MOUNT_DISK_UNMOUNTED) {
    web_ui_->CallJavascriptFunction("rootsChanged");
  }
}

void ImageBurnHandler::DeviceChanged(chromeos::MountLibraryEventType event,
                                     const std::string& device_path) {
  if (event == chromeos::MOUNT_DEVICE_REMOVED)
    web_ui_->CallJavascriptFunction("rootsChanged");
}


void ImageBurnHandler::ProgressUpdated(chromeos::BurnLibrary* object,
                                       chromeos::BurnEventType evt,
                                       const ImageBurnStatus& status) {
  UpdateBurnProgress(status.amount_burnt, status.total_size,
                     status.target_path, evt);
  if (evt == chromeos::BURN_COMPLETE) {
    FinalizeBurn(true);
  } else if (evt == chromeos::BURN_CANCELED) {
    FinalizeBurn(false);
  }
}

void ImageBurnHandler::OnDownloadUpdated(DownloadItem* download) {
  if (download->IsCancelled()) {
    DownloadCompleted(false);  // Should stop observation.
    DCHECK(!download_item_observer_added_);
  } else if (download->IsComplete()) {
    zip_image_file_path_ = download->full_path();
    DownloadCompleted(true);  // Should stop observation.
    DCHECK(!download_item_observer_added_);
  } else if (download->IsPartialDownload()) {
      scoped_ptr<DictionaryValue> result_value(
          download_util::CreateDownloadItemValue(download, 0));
      web_ui_->CallJavascriptFunction("downloadUpdated", *result_value);
  }
}

void ImageBurnHandler::OnDownloadOpened(DownloadItem* download) {
  if (download->safety_state() == DownloadItem::DANGEROUS)
    download->DangerousDownloadValidated();
}

void ImageBurnHandler::ModelChanged() {
  // Find our item and observe it.
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(
      resource_manager_->GetImageDir(), &downloads);
  if (download_item_observer_added_)
    return;
  for (std::vector<DownloadItem*>::const_iterator it = downloads.begin();
      it != downloads.end();
      ++it) {
    if ((*it)->original_url() == *image_download_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void ImageBurnHandler::OnImageDirCreated(bool success,
                                         ImageBurnTaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Transfer to UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task, &ImageBurnTaskProxy::OnImageDirCreated,
                        success));
}

void ImageBurnHandler::OnDownloadStarted(bool success) {
  if (success)
    resource_manager_->set_download_started(true);
  else
    DownloadCompleted(false);
}

void ImageBurnHandler::HandleGetRoots(const ListValue* args) {
  ListValue results_value;
  DictionaryValue info_value;
  chromeos::MountLibrary* mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  const chromeos::MountLibrary::DiskMap& disks = mount_lib->disks();
  if (!resource_manager_->burn_in_progress()) {
    for (chromeos::MountLibrary::DiskMap::const_iterator iter =  disks.begin();
         iter != disks.end();
         ++iter) {
      if (iter->second->is_parent()) {
        FilePath disk_path = FilePath(iter->second->system_path()).DirName();
        std::string title = "/dev/" + disk_path.BaseName().value();
        if (!iter->second->on_boot_device()) {
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
  web_ui_->CallJavascriptFunction("browseFileResult",
                                  info_value, results_value);
}

void ImageBurnHandler::HandleDownloadImage(const ListValue* args) {
  ExtractTargetedDeviceSystemPath(args);
  if (resource_manager_->GetImageDir().empty()) {
    // Create image dir on File thread.
    scoped_refptr<ImageBurnTaskProxy> task =
        new ImageBurnTaskProxy(AsWeakPtr());
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(task.get(), &ImageBurnTaskProxy::CreateImageDir));
  } else {
    OnImageDirCreatedOnUIThread(true);
  }
}

void ImageBurnHandler::DownloadCompleted(bool success) {
  resource_manager_->SetDownloadFinished(success);
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  if (success) {
    UnzipImage();
  } else {
    UnzipComplete(success);
  }
}

void ImageBurnHandler::UnzipComplete(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DictionaryValue signal_value;
  if (success) {
    signal_value.SetString("state", "COMPLETE");
    web_ui_->CallJavascriptFunction("downloadUpdated", signal_value);
    web_ui_->CallJavascriptFunction("promptUserDownloadFinished");
  } else {
    signal_value.SetString("state", "CANCELLED");
    web_ui_->CallJavascriptFunction("downloadUpdated", signal_value);
    web_ui_->CallJavascriptFunction("alertUserDownloadAborted");
  }
}

void ImageBurnHandler::HandleBurnImage(const ListValue* args) {
  scoped_refptr<ImageBurnTaskProxy> task = new ImageBurnTaskProxy(AsWeakPtr());
  BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(task.get(), &ImageBurnTaskProxy::BurnImage));
}

void ImageBurnHandler::HandleCancelBurnImage(const ListValue* args) {
  image_target_.clear();
}

void ImageBurnHandler::CreateImageDirOnFileThread(ImageBurnTaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  resource_manager_->CreateImageDir(this, task);
}

void ImageBurnHandler::OnImageDirCreatedOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success) {
    zip_image_file_path_ =
        resource_manager_->GetImageDir().Append(kImageFileName);
    resource_manager_->CreateImageUrl(tab_contents_, this);
  } else {
    DownloadCompleted(success);
  }
}

void ImageBurnHandler::BurnImageOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  if (resource_manager_->burn_in_progress())
    return;
  resource_manager_->set_burn_in_progress(true);

  if (chromeos::CrosLibrary::Get()->GetBurnLibrary()->
      DoBurn(image_file_path_, image_target_)) {
    DictionaryValue signal_value;
    signal_value.SetString("state", "IN_PROGRESS");
    signal_value.SetString("path", image_target_.value());
    signal_value.SetInteger("received", 0);
    signal_value.SetString("progress_status_text", "");
    web_ui_->CallJavascriptFunction("burnProgressUpdated", signal_value);
  }
}

void ImageBurnHandler::FinalizeBurn(bool successful) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui_->CallJavascriptFunction(successful ? "burnSuccessful"
                                             : "burnUnsuccessful");
  resource_manager_->set_burn_in_progress(false);
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

  web_ui_->CallJavascriptFunction("burnProgressUpdated", progress_value);
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

void ImageBurnHandler::OnImageUrlCreated(GURL* image_url, bool success) {
  if (!success) {
    DownloadCompleted(false);
    return;
  }
  image_download_url_ = image_url;

  download_manager_ = tab_contents_->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);

  if (!resource_manager_->download_started()) {
    resource_manager_->set_download_started(true);
    if (!resource_manager_->image_download_requested()) {
      resource_manager_->set_image_download_requested(true);
      ImageBurnDownloader::GetInstance()->AddListener(this,
          *image_download_url_);
      ImageBurnDownloader::GetInstance()->DownloadFile(*image_download_url_,
                                                       zip_image_file_path_,
                                                       tab_contents_);
    }
  } else if (resource_manager_->download_finished()) {
    DownloadCompleted(true);
  }
}

void ImageBurnHandler::ExtractTargetedDeviceSystemPath(
    const ListValue* list_value) {
  Value* list_member;
  if (list_value->Get(0, &list_member) &&
      list_member->GetType() == Value::TYPE_STRING) {
    const StringValue* string_value =
        static_cast<const StringValue*>(list_member);
    std::string image_dest;
    string_value->GetAsString(&image_dest);
    image_target_ = FilePath(image_dest);
  } else {
    LOG(ERROR) << "Unable to get path string";
  }
}

void ImageBurnHandler::UnzipImage() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_refptr<ImageBurnTaskProxy> task = new ImageBurnTaskProxy(AsWeakPtr());
  BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(task.get(), &ImageBurnTaskProxy::UnzipImage));
}

void ImageBurnHandler::UnzipImageOnFileThread(ImageBurnTaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool success = UnzipImageImpl();
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(task, &ImageBurnTaskProxy::UnzipComplete, success));
}

bool ImageBurnHandler::UnzipImageImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  const FilePath& img_dir = resource_manager_->GetImageDir();
  if (!Unzip(zip_image_file_path_, img_dir))
    return false;

  image_file_path_.clear();
  file_util::FileEnumerator file_enumerator(
      img_dir, false,  // recursive
      file_util::FileEnumerator::FILES);
  for (FilePath path = file_enumerator.Next();
      !path.empty();
       path = file_enumerator.Next()) {
    if (path != zip_image_file_path_) {
      image_file_path_ = path;
      break;
    }
  }
  return !image_file_path_.empty();
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnResourceManager
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnResourceManager::ImageBurnResourceManager()
    : image_download_requested_(false),
      download_started_(false),
      download_finished_(false),
      burn_in_progress_(false),
      download_manager_(NULL),
      download_item_observer_added_(false),
      active_download_item_(NULL),
      image_url_(new GURL(kImageDownloadURL)),
      config_file_url_(std::string(kImageBaseURL) + kImageFetcherName),
      config_file_requested_(false),
      config_file_fetched_(true) {
  image_dir_.clear();
}

ImageBurnResourceManager::~ImageBurnResourceManager() {
  if (!image_dir_.empty()) {
    file_util::Delete(image_dir_, true);
  }
  if (active_download_item_)
    active_download_item_->RemoveObserver(this);
  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

// static
ImageBurnResourceManager* ImageBurnResourceManager::GetInstance() {
  return Singleton<ImageBurnResourceManager>::get();
}

void ImageBurnResourceManager::OnDownloadUpdated(DownloadItem* download) {
  if (download->IsCancelled()) {
    image_url_.reset();
    ConfigFileFetched(false);

    // ConfigFileFetched should remove observer.
    DCHECK(!download_item_observer_added_);
    DCHECK(active_download_item_ == NULL);
  } else if (download->IsComplete()) {
    std::string image_url;
    if (file_util::ReadFileToString(config_file_path_, &image_url)) {
      image_url_.reset(new GURL(std::string(kImageBaseURL) + image_url));
      ConfigFileFetched(true);
    } else {
      image_url_.reset();
      ConfigFileFetched(false);
    }
  }
}

void ImageBurnResourceManager::ModelChanged() {
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(GetImageDir(), &downloads);
  if (download_item_observer_added_)
    return;
  for (std::vector<DownloadItem*>::const_iterator it = downloads.begin();
      it != downloads.end();
      ++it) {
    if ((*it)->url() == config_file_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void ImageBurnResourceManager::OnDownloadStarted(bool success) {
  if (!success)
    ConfigFileFetched(false);
}

void ImageBurnResourceManager::CreateImageDir(
    Delegate* delegate,
    ImageBurnTaskProxy* task) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool success = true;
  if (image_dir_.empty()) {
    CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &image_dir_));
    image_dir_ = image_dir_.Append(kTempImageFolderName);
    success = file_util::CreateDirectory(image_dir_);
  }
  delegate->OnImageDirCreated(success, task);
}

const FilePath& ImageBurnResourceManager::GetImageDir() {
  return image_dir_;
}

void ImageBurnResourceManager::SetDownloadFinished(bool finished) {
  if (!download_started_)
    return;
  if (!finished)
    download_started_ = false;
  download_finished_ = finished;
}

void ImageBurnResourceManager::CreateImageUrl(TabContents* tab_contents,
    Delegate* delegate) {
  if (config_file_fetched_) {
    delegate->OnImageUrlCreated(image_url_.get(), true);
    return;
  }
  downloaders_.push_back(delegate);

  if (config_file_requested_)
    return;
  config_file_requested_ = true;

  config_file_path_ = GetImageDir().Append(kImageFetcherName);

  download_manager_ = tab_contents->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);

  ImageBurnDownloader* downloader = ImageBurnDownloader::GetInstance();
  downloader->AddListener(this, config_file_url_);
  downloader->DownloadFile(config_file_url_, config_file_path_, tab_contents);
}

void ImageBurnResourceManager::ConfigFileFetched(bool fetched) {
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_)
    download_manager_->RemoveObserver(this);
  if (!fetched)
    config_file_requested_ = false;
  config_file_fetched_ = fetched;
  for (size_t i = 0; i < downloaders_.size(); ++i)
    downloaders_[i]->OnImageUrlCreated(image_url_.get(), fetched);
  downloaders_.clear();
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnDownloaderTaskProxy
//
////////////////////////////////////////////////////////////////////////////////

class ImageBurnDownloaderTaskProxy
    : public base::RefCountedThreadSafe<ImageBurnDownloaderTaskProxy> {
 public:
  explicit ImageBurnDownloaderTaskProxy() {}

  void CreateFileStream(const GURL& url,
                        const FilePath& target_path,
                        TabContents* tab_contents) {
    ImageBurnDownloader::GetInstance()->CreateFileStreamOnFileThread(url,
        target_path, tab_contents, this);
  }

  void OnFileStreamCreated(const GURL& url,
                           const FilePath& file_path,
                           TabContents* tab_contents,
                           net::FileStream* created_file_stream) {
    ImageBurnDownloader::GetInstance()->OnFileStreamCreatedOnUIThread(url,
        file_path, tab_contents, created_file_stream);
  }

 private:
  ~ImageBurnDownloaderTaskProxy() {}

  friend class base::RefCountedThreadSafe<ImageBurnDownloaderTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnDownloaderTaskProxy);
};

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnDownloader
//
////////////////////////////////////////////////////////////////////////////////

// static
ImageBurnDownloader* ImageBurnDownloader::GetInstance() {
  return Singleton<ImageBurnDownloader>::get();
}

void ImageBurnDownloader::DownloadFile(const GURL& url,
    const FilePath& file_path, TabContents* tab_contents) {
  // First we have to create file stream we will download file to.
  // That has to be done on File thread.
  scoped_refptr<ImageBurnDownloaderTaskProxy> task =
      new ImageBurnDownloaderTaskProxy();
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(task.get(),
          &ImageBurnDownloaderTaskProxy::CreateFileStream, url, file_path,
          tab_contents));
}

void ImageBurnDownloader::CreateFileStreamOnFileThread(
    const GURL& url, const FilePath& file_path,
    TabContents* tab_contents, ImageBurnDownloaderTaskProxy* task) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!file_path.empty());

  scoped_ptr<net::FileStream> file_stream(new net::FileStream);
  if (file_stream->Open(file_path, base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE))
    file_stream.reset(NULL);

  // Call callback method on UI thread.
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(task,
            &ImageBurnDownloaderTaskProxy::OnFileStreamCreated,
            url, file_path, tab_contents, file_stream.release()));
}

void ImageBurnDownloader::OnFileStreamCreatedOnUIThread(const GURL& url,
    const FilePath& file_path, TabContents* tab_contents,
    net::FileStream* created_file_stream) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (created_file_stream) {
    DownloadManager* download_manager =
        tab_contents->profile()->GetDownloadManager();
    DownloadSaveInfo save_info;
    save_info.file_path = file_path;
    save_info.file_stream = linked_ptr<net::FileStream>(created_file_stream);
    DownloadStarted(true, url);
    download_manager->DownloadUrlToFile(url,
                                        tab_contents->GetURL(),
                                        tab_contents->encoding(),
                                        save_info,
                                        tab_contents);
  } else {
    DownloadStarted(false, url);
  }
}

void ImageBurnDownloader::AddListener(Listener* listener, const GURL& url) {
  listeners_.insert(std::make_pair(url, listener));
}

void ImageBurnDownloader::DownloadStarted(bool success, const GURL& url) {
  std::pair<ListenerMap::iterator, ListenerMap::iterator> listener_range =
      listeners_.equal_range(url);
  for (ListenerMap::iterator current_listener = listener_range.first;
       current_listener != listener_range.second;
       ++current_listener) {
    current_listener->second->OnDownloadStarted(success);
  }
  listeners_.erase(listener_range.first, listener_range.second);
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnUI
//
////////////////////////////////////////////////////////////////////////////////
ImageBurnUI::ImageBurnUI(TabContents* contents) : WebUI(contents) {
  ImageBurnHandler* handler = new ImageBurnHandler(contents);
  AddMessageHandler((handler)->Attach(this));
  ImageBurnUIHTMLSource* html_source = new ImageBurnUIHTMLSource();
  contents->profile()->GetChromeURLDataManager()->AddDataSource(html_source);
}
