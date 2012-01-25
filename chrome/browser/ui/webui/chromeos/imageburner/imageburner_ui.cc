// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/i18n/rtl.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/burn_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/disks/disk_mount_manager.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_utils.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "googleurl/src/gurl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/bytes_formatting.h"


using content::BrowserThread;
using content::DownloadItem;
using content::WebContents;

namespace imageburner {

namespace {

// An enum used to describe what type of progress is made.
enum ProgressType {
  DOWNLOAD,
  UNZIP,
  BURN
};

const char kPropertyDevicePath[] = "devicePath";
const char kPropertyFilePath[] = "filePath";
const char kPropertyLabel[] = "label";
const char kPropertyPath[] = "path";

// Name for hwid in machine statistics.
const char kHwidStatistic[] = "hardware_class";

const char kImageZipFileName[] = "chromeos_image.bin.zip";

// 3.9GB. It is less than 4GB because true device size ussually varies a little.
const uint64 kMinDeviceSize = static_cast<uint64>(3.9 * 1000 * 1000 * 1000);

// Link displayed on imageburner ui.
const char kMoreInfoLink[] =
    "http://www.chromium.org/chromium-os/chromiumos-design-docs/recovery-mode";

ChromeWebUIDataSource* CreateImageburnerUIHTMLSource() {
  ChromeWebUIDataSource* source =
      new ChromeWebUIDataSource(chrome::kChromeUIImageBurnerHost);

  source->AddLocalizedString("headerTitle", IDS_IMAGEBURN_HEADER_TITLE);
  source->AddLocalizedString("headerDescription",
                             IDS_IMAGEBURN_HEADER_DESCRIPTION);
  source->AddLocalizedString("headerLink", IDS_IMAGEBURN_HEADER_LINK);
  source->AddLocalizedString("statusDevicesNone",
                             IDS_IMAGEBURN_NO_DEVICES_STATUS);
  source->AddLocalizedString("warningDevicesNone",
                             IDS_IMAGEBURN_NO_DEVICES_WARNING);
  source->AddLocalizedString("statusDevicesMultiple",
                             IDS_IMAGEBURN_MUL_DEVICES_STATUS);
  source->AddLocalizedString("statusDeviceUSB",
                             IDS_IMAGEBURN_USB_DEVICE_STATUS);
  source->AddLocalizedString("statusDeviceSD",
                             IDS_IMAGEBURN_SD_DEVICE_STATUS);
  source->AddLocalizedString("warningDevices",
                             IDS_IMAGEBURN_DEVICES_WARNING);
  source->AddLocalizedString("statusNoConnection",
                             IDS_IMAGEBURN_NO_CONNECTION_STATUS);
  source->AddLocalizedString("warningNoConnection",
                             IDS_IMAGEBURN_NO_CONNECTION_WARNING);
  source->AddLocalizedString("statusNoSpace",
                             IDS_IMAGEBURN_INSUFFICIENT_SPACE_STATUS);
  source->AddLocalizedString("warningNoSpace",
                             IDS_IMAGEBURN_INSUFFICIENT_SPACE_WARNING);
  source->AddLocalizedString("statusDownloading",
                             IDS_IMAGEBURN_DOWNLOADING_STATUS);
  source->AddLocalizedString("statusUnzip", IDS_IMAGEBURN_UNZIP_STATUS);
  source->AddLocalizedString("statusBurn", IDS_IMAGEBURN_BURN_STATUS);
  source->AddLocalizedString("statusError", IDS_IMAGEBURN_ERROR_STATUS);
  source->AddLocalizedString("statusSuccess", IDS_IMAGEBURN_SUCCESS_STATUS);
  source->AddLocalizedString("warningSuccess", IDS_IMAGEBURN_SUCCESS_DESC);
  source->AddLocalizedString("title", IDS_IMAGEBURN_PAGE_TITLE);
  source->AddLocalizedString("confirmButton", IDS_IMAGEBURN_CONFIRM_BUTTON);
  source->AddLocalizedString("cancelButton", IDS_IMAGEBURN_CANCEL_BUTTON);
  source->AddLocalizedString("retryButton", IDS_IMAGEBURN_RETRY_BUTTON);
  source->AddString("moreInfoLink", ASCIIToUTF16(kMoreInfoLink));

  source->set_json_path("strings.js");
  source->add_resource_path("image_burner.js", IDR_IMAGEBURNER_JS);
  source->set_default_resource(IDR_IMAGEBURNER_HTML);
  return source;
}


////////////////////////////////////////////////////////////////////////////////
//
// WebUIHandlerTaskProxy
//
////////////////////////////////////////////////////////////////////////////////

class WebUIHandlerTaskProxy
    : public base::RefCountedThreadSafe<WebUIHandlerTaskProxy> {
 public:
  class Delegate : public base::SupportsWeakPtr<Delegate> {
   public:
    virtual void CreateImageDirOnFileThread() = 0;
    virtual void ImageDirCreatedOnUIThread(bool success) = 0;
  };

  explicit WebUIHandlerTaskProxy(Delegate* delegate) {
    delegate_ = delegate->AsWeakPtr();
    delegate_->DetachFromThread();
  }

  void CreateImageDir() {
    if (delegate_)
      delegate_->CreateImageDirOnFileThread();
  }

  void OnImageDirCreated(bool success) {
    if (delegate_)
      delegate_->ImageDirCreatedOnUIThread(success);
  }

  // WebUIHandlerTaskProxy is created on the UI thread, so in some cases,
  // we need to post back to the UI thread for destruction.
  void DeleteOnUIThread() {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(&base::DoNothing));
  }

 private:
  base::WeakPtr<Delegate> delegate_;

  friend class base::RefCountedThreadSafe<WebUIHandlerTaskProxy>;
  ~WebUIHandlerTaskProxy() {
  }

  DISALLOW_COPY_AND_ASSIGN(WebUIHandlerTaskProxy);
};


////////////////////////////////////////////////////////////////////////////////
//
// WebUIHandler
//
////////////////////////////////////////////////////////////////////////////////

class WebUIHandler
    : public content::WebUIMessageHandler,
      public chromeos::disks::DiskMountManager::Observer,
      public chromeos::BurnLibrary::Observer,
      public chromeos::NetworkLibrary::NetworkManagerObserver,
      public content::DownloadItem::Observer,
      public content::DownloadManager::Observer,
      public Downloader::Listener,
      public StateMachine::Observer,
      public WebUIHandlerTaskProxy::Delegate,
      public BurnManager::Delegate {
 public:
  explicit WebUIHandler(content::WebContents* contents)
      : web_contents_(contents),
        download_manager_(NULL),
        active_download_item_(NULL),
        burn_manager_(NULL),
        state_machine_(NULL),
        observing_burn_lib_(false),
        working_(false) {
    chromeos::disks::DiskMountManager::GetInstance()->AddObserver(this);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        AddNetworkManagerObserver(this);
    burn_manager_ = BurnManager::GetInstance();
    state_machine_ = burn_manager_->state_machine();
    state_machine_->AddObserver(this);
  }

  virtual ~WebUIHandler() {
    chromeos::disks::DiskMountManager::GetInstance()->
        RemoveObserver(this);
    chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
    chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        RemoveNetworkManagerObserver(this);
    CleanupDownloadObjects();
    if (state_machine_)
      state_machine_->RemoveObserver(this);
  }

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE {
    web_ui()->RegisterMessageCallback(
        "getDevices",
        base::Bind(&WebUIHandler::HandleGetDevices, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "burnImage",
        base::Bind(&WebUIHandler::HandleBurnImage, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "cancelBurnImage",
        base::Bind(&WebUIHandler::HandleCancelBurnImage,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "webuiInitialized",
        base::Bind(&WebUIHandler::HandleWebUIInitialized,
                   base::Unretained(this)));
  }

  // chromeos::disks::DiskMountManager::Observer interface.
  virtual void DiskChanged(chromeos::disks::DiskMountManagerEventType event,
                           const chromeos::disks::DiskMountManager::Disk* disk)
      OVERRIDE {
    if (!disk->is_parent() || disk->on_boot_device())
      return;
    if (event == chromeos::disks::MOUNT_DISK_ADDED) {
      DictionaryValue disk_value;
      CreateDiskValue(*disk, &disk_value);
      web_ui()->CallJavascriptFunction("browserBridge.deviceAdded", disk_value);
    } else if (event == chromeos::disks::MOUNT_DISK_REMOVED) {
      StringValue device_path_value(disk->device_path());
      web_ui()->CallJavascriptFunction("browserBridge.deviceRemoved",
                                       device_path_value);
      if (burn_manager_->target_device_path().value() ==
          disk->device_path()) {
        ProcessError(IDS_IMAGEBURN_DEVICE_NOT_FOUND_ERROR);
      }
    }
  }

  virtual void DeviceChanged(chromeos::disks::DiskMountManagerEventType event,
                             const std::string& device_path) OVERRIDE {
  }

  virtual void MountCompleted(
      chromeos::disks::DiskMountManager::MountEvent event_type,
      chromeos::MountError error_code,
      const chromeos::disks::DiskMountManager::MountPointInfo& mount_info)
      OVERRIDE {
  }

  // chromeos::BurnLibrary::Observer interface.
  virtual void BurnProgressUpdated(chromeos::BurnLibrary* object,
                                   chromeos::BurnEvent evt,
                                   const ImageBurnStatus& status) OVERRIDE {
    switch (evt) {
      case(chromeos::BURN_SUCCESS):
        FinalizeBurn();
        break;
      case(chromeos::BURN_FAIL):
        ProcessError(IDS_IMAGEBURN_BURN_ERROR);
        break;
      case(chromeos::BURN_UPDATE):
        SendProgressSignal(BURN, status.amount_burnt, status.total_size, NULL);
        break;
      case(chromeos::UNZIP_STARTED):
        SendProgressSignal(UNZIP, 0, 0, NULL);
        break;
      case(chromeos::UNZIP_FAIL):
        ProcessError(IDS_IMAGEBURN_EXTRACTING_ERROR);
        break;
      case(chromeos::UNZIP_COMPLETE):
        // We ignore this.
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // chromeos::NetworkLibrary::NetworkManagerObserver interface.
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* obj) OVERRIDE {
    if (state_machine_->state() == StateMachine::INITIAL && CheckNetwork()) {
      web_ui()->CallJavascriptFunction("browserBridge.reportNetworkDetected");
    }
    if (state_machine_->state() == StateMachine::DOWNLOADING &&
        !CheckNetwork()) {
      ProcessError(IDS_IMAGEBURN_NETWORK_ERROR);
    }
  }

  // DownloadItem::Observer interface.
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE {
    if (download->IsCancelled()) {
      DownloadCompleted(false);
      DCHECK(!active_download_item_);
    } else if (download->IsComplete()) {
      burn_manager_->set_final_zip_file_path(download->GetFullPath());
      DownloadCompleted(true);
      DCHECK(!active_download_item_);
    } else if (download->IsPartialDownload() &&
               state_machine_->state() == StateMachine::DOWNLOADING) {
      base::TimeDelta remaining_time;
      download->TimeRemaining(&remaining_time);
      SendProgressSignal(DOWNLOAD, download->GetReceivedBytes(),
                         download->GetTotalBytes(), &remaining_time);
    }
  }

  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE {
    if (download->GetSafetyState() == DownloadItem::DANGEROUS)
      download->DangerousDownloadValidated();
  }

  // DownloadManager::Observer interface.
  virtual void ModelChanged() OVERRIDE {
    // Find our item and observe it.
    std::vector<DownloadItem*> downloads;
    download_manager_->GetTemporaryDownloads(
        burn_manager_->GetImageDir(), &downloads);
    if (active_download_item_)
      return;
    for (std::vector<DownloadItem*>::const_iterator it = downloads.begin();
         it != downloads.end();
         ++it) {
      if ((*it)->GetOriginalUrl() == image_download_url_) {
        (*it)->AddObserver(this);
        active_download_item_ = *it;
        break;
      }
    }
  }

  // Downloader::Listener interface.
  virtual void OnBurnDownloadStarted(bool success) OVERRIDE {
    if (success)
      state_machine_->OnDownloadStarted();
    else
      DownloadCompleted(false);
  }

  // StateMachine::Observer interface.
  virtual void OnBurnStateChanged(StateMachine::State state)
      OVERRIDE {
    if (state == StateMachine::CANCELLED) {
      ProcessError(IDS_IMAGEBURN_USER_ERROR);
    } else if (state != StateMachine::INITIAL && !working_) {
      // User has started burn process, so let's start observing.
      HandleBurnImage(NULL);
    }
  }

  virtual void OnError(int error_message_id) OVERRIDE {
    StringValue error_message(l10n_util::GetStringUTF16(error_message_id));
    web_ui()->CallJavascriptFunction("browserBridge.reportFail", error_message);
    working_ = false;
  }

  // Part of WebUIHandlerTaskProxy::Delegate interface.
  virtual void CreateImageDirOnFileThread() OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    burn_manager_->CreateImageDir(this);
  }

  // Part of BurnManager::Delegate interface.
  virtual void OnImageDirCreated(bool success) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    // Transfer to UI thread.
    scoped_refptr<WebUIHandlerTaskProxy> task = new WebUIHandlerTaskProxy(this);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&WebUIHandlerTaskProxy::OnImageDirCreated,
                   task.get(), success));
  }

  // Part of WebUIHandlerTaskProxy::Delegate interface.
  virtual void ImageDirCreatedOnUIThread(bool success) OVERRIDE {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (success) {
      zip_image_file_path_ =
          burn_manager_->GetImageDir().Append(kImageZipFileName);
      burn_manager_->FetchConfigFile(web_contents_, this);
    } else {
      DownloadCompleted(success);
    }
  }

  // Part of BurnManager::Delegate interface.
  virtual void OnConfigFileFetched(const ConfigFile& config_file, bool success)
      OVERRIDE {
    if (!success || !ExtractInfoFromConfigFile(config_file)) {
      DownloadCompleted(false);
      return;
    }

    if (state_machine_->download_finished()) {
      BurnImage();
      return;
    }

    if (!download_manager_) {
      download_manager_ =
          web_contents_->GetBrowserContext()->GetDownloadManager();
      download_manager_->AddObserver(this);
    }
    if (!state_machine_->download_started()) {
      burn_manager_->downloader()->AddListener(this, image_download_url_);
      if (!state_machine_->image_download_requested()) {
        state_machine_->OnImageDownloadRequested();
        burn_manager_->downloader()->DownloadFile(image_download_url_,
                                                  zip_image_file_path_,
                                                  web_contents_);
      }
    }
  }

 private:
  void CreateDiskValue(const chromeos::disks::DiskMountManager::Disk& disk,
                       DictionaryValue* disk_value) {
    string16 label = ASCIIToUTF16(disk.drive_label());
    base::i18n::AdjustStringForLocaleDirection(&label);
    disk_value->SetString(std::string(kPropertyLabel), label);
    disk_value->SetString(std::string(kPropertyFilePath), disk.file_path());
    disk_value->SetString(std::string(kPropertyDevicePath), disk.device_path());
  }

  // Callback for the "getRoots" message.
  void HandleGetDevices(const ListValue* args) {
    chromeos::disks::DiskMountManager* disk_mount_manager =
        chromeos::disks::DiskMountManager::GetInstance();
    const chromeos::disks::DiskMountManager::DiskMap& disks =
        disk_mount_manager->disks();
    ListValue results_value;
    for (chromeos::disks::DiskMountManager::DiskMap::const_iterator iter =
             disks.begin();
         iter != disks.end();
         ++iter) {
      chromeos::disks::DiskMountManager::Disk* disk = iter->second;
      if (disk->is_parent() && !disk->on_boot_device()) {
        DictionaryValue* disk_value = new DictionaryValue();
        CreateDiskValue(*disk, disk_value);
        results_value.Append(disk_value);
      }
    }
    web_ui()->CallJavascriptFunction("browserBridge.getDevicesCallback",
                                     results_value);
  }

  // Callback for the webuiInitialized message.
  void HandleWebUIInitialized(const ListValue* args) {
    if (state_machine_->state() == StateMachine::BURNING) {
      // There is nothing else left to do but observe burn progress.
      BurnImage();
    } else if (state_machine_->state() != StateMachine::INITIAL) {
      // User has started burn process, so let's start observing.
      HandleBurnImage(NULL);
    }
  }

  // Callback for the "cancelBurnImage" message.
  void HandleCancelBurnImage(const ListValue* args) {
    state_machine_->OnCancelation();
  }

  // Callback for the "burnImage" message.
  // It may be called with NULL if there is a handler that has started burning,
  // and thus set the target paths.
  void HandleBurnImage(const ListValue* args) {
    if (args && state_machine_->new_burn_posible()) {
      if (!CheckNetwork()) {
        web_ui()->CallJavascriptFunction("browserBridge.reportNoNetwork");
        return;
      }
      FilePath target_device_path;
      ExtractTargetedDevicePath(*args, 0, &target_device_path);
      burn_manager_->set_target_device_path(target_device_path);

      FilePath target_file_path;
      ExtractTargetedDevicePath(*args, 1, &target_file_path);
      burn_manager_->set_target_file_path(target_file_path);

      uint64 device_size = GetDeviceSize(
          burn_manager_->target_device_path().value());
      if (device_size < kMinDeviceSize) {
        SendDeviceTooSmallSignal(device_size);
        return;
      }
    }
    if (working_)
      return;
    working_ = true;
    // Send progress signal now so ui doesn't hang in intial state until we get
    // config file
    SendProgressSignal(DOWNLOAD, 0, 0, NULL);
    if (burn_manager_->GetImageDir().empty()) {
      // Create image dir on File thread.
      scoped_refptr<WebUIHandlerTaskProxy> task =
          new WebUIHandlerTaskProxy(this);
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&WebUIHandlerTaskProxy::CreateImageDir, task.get()));
    } else {
      ImageDirCreatedOnUIThread(true);
    }
  }

 private:
  void DownloadCompleted(bool success) {
    CleanupDownloadObjects();
    if (success) {
      state_machine_->OnDownloadFinished();
      BurnImage();
    } else {
      ProcessError(IDS_IMAGEBURN_DOWNLOAD_ERROR);
    }
  }

  void BurnImage() {
    if (!observing_burn_lib_) {
      chromeos::CrosLibrary::Get()->GetBurnLibrary()->AddObserver(this);
      observing_burn_lib_ = true;
    }
    if (state_machine_->state() == StateMachine::BURNING)
      return;
    state_machine_->OnBurnStarted();

    chromeos::CrosLibrary::Get()->GetBurnLibrary()->DoBurn(
        zip_image_file_path_,
        image_file_name_, burn_manager_->target_file_path(),
        burn_manager_->target_device_path());
  }

  void FinalizeBurn() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    state_machine_->OnSuccess();
    burn_manager_->ResetTargetPaths();
    chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
    observing_burn_lib_ = false;
    web_ui()->CallJavascriptFunction("browserBridge.reportSuccess");
    working_ = false;
  }

  // Error is ussually detected by all existing Burn handlers, but only first
  // one that calls ProcessError should actually process it.
  void ProcessError(int message_id) {
    // If we are in intial state, error has already been dispached.
    if (state_machine_->state() == StateMachine::INITIAL) {
      // We don't need burn library since we are not the ones doing the cleanup.
      if (observing_burn_lib_) {
        chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
        observing_burn_lib_ = false;
      }
      return;
    }

    // Remember burner state, since it will be reset after OnError call.
    StateMachine::State state = state_machine_->state();

    // Dispach error. All hadlers' OnError event will be called before returning
    // from this. This includes us, too.
    state_machine_->OnError(message_id);

    // Do cleanup.
    if (state  == StateMachine::DOWNLOADING) {
      if (active_download_item_) {
        // This will trigger Download canceled event. As a response to that
        // event, handlers will remove themselves as observers from download
        // manager and item.
        // We don't want to process Download Cancelled signal.
        active_download_item_->RemoveObserver(this);
        if (active_download_item_->IsPartialDownload())
          active_download_item_->Cancel(true);
        active_download_item_->Delete(DownloadItem::DELETE_DUE_TO_USER_DISCARD);
        active_download_item_ = NULL;
        CleanupDownloadObjects();
      }
    } else if (state == StateMachine::BURNING) {
      DCHECK(observing_burn_lib_);
      // Burn library doesn't send cancelled signal upon CancelBurnImage
      // invokation.
      chromeos::CrosLibrary::Get()->GetBurnLibrary()->CancelBurnImage();
      chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
      observing_burn_lib_ = false;
    }
    burn_manager_->ResetTargetPaths();
  }

  bool ExtractInfoFromConfigFile(const ConfigFile& config_file) {
    std::string hwid;
    if (!chromeos::system::StatisticsProvider::GetInstance()->
        GetMachineStatistic(kHwidStatistic, &hwid))
      return false;

    image_file_name_ = config_file.GetProperty(kFileName, hwid);
    if (image_file_name_.empty())
      return false;

    image_download_url_ = GURL(config_file.GetProperty(kUrl, hwid));
    if (image_download_url_.is_empty()) {
      image_file_name_.clear();
      return false;
    }

    return true;
  }

  void CleanupDownloadObjects() {
    if (active_download_item_) {
      active_download_item_->RemoveObserver(this);
      active_download_item_ = NULL;
    }
    if (download_manager_) {
      download_manager_->RemoveObserver(this);
      download_manager_ = NULL;
    }
  }

  void SendDeviceTooSmallSignal(int64 device_size) {
    string16 size;
    GetDataSizeText(device_size, &size);
    StringValue device_size_text(size);
    web_ui()->CallJavascriptFunction("browserBridge.reportDeviceTooSmall",
                                     device_size_text);
  }

  void SendProgressSignal(ProgressType progress_type, int64 amount_finished,
                          int64 amount_total, const base::TimeDelta* time) {
    DictionaryValue progress;
    int progress_message_id = 0;
    int time_left_message_id = IDS_IMAGEBURN_PROGRESS_TIME_UNKNOWN;
    if (time)
      time_left_message_id = IDS_IMAGEBURN_DOWNLOAD_TIME_REMAINING;
    switch (progress_type) {
      case(DOWNLOAD):
        progress.SetString("progressType", "download");
        progress_message_id = IDS_IMAGEBURN_DOWNLOAD_PROGRESS_TEXT;
        break;
      case(UNZIP):
        progress.SetString("progressType", "unzip");
        break;
      case(BURN):
        progress.SetString("progressType", "burn");
        progress_message_id = IDS_IMAGEBURN_BURN_PROGRESS_TEXT;
        break;
      default:
        return;
    }

    progress.SetInteger("amountFinished", amount_finished);
    progress.SetInteger("amountTotal", amount_total);
    if (amount_total != 0) {
      string16 progress_text;
      GetProgressText(progress_message_id, amount_finished, amount_total,
                      &progress_text);
      progress.SetString("progressText", progress_text);
    } else {
      progress.SetString("progressText", "");
    }

    string16 time_left_text;
    GetProgressTimeLeftText(time_left_message_id, time, &time_left_text);
    progress.SetString("timeLeftText", time_left_text);

    web_ui()->CallJavascriptFunction("browserBridge.updateProgress", progress);
  }

  // time_left_text should be previously created.
  void GetProgressTimeLeftText(int message_id,
                               const base::TimeDelta* time_left,
                               string16* time_left_text) {
    if (time_left) {
      string16 time_remaining_str = TimeFormat::TimeRemaining(*time_left);
      *time_left_text =
          l10n_util::GetStringFUTF16(message_id, time_remaining_str);
    } else {
      *time_left_text = l10n_util::GetStringUTF16(message_id);
    }
  }

  // size_text should be previously created.
  void GetDataSizeText(int64 size, string16* size_text) {
    *size_text = ui::FormatBytes(size);
    base::i18n::AdjustStringForLocaleDirection(size_text);
  }

  // progress_text should be previously created.
  void GetProgressText(int message_id, int64 amount_finished,
                       int64 amount_total, string16* progress_text) {
    string16 finished;
    GetDataSizeText(amount_finished, &finished);
    string16 total;
    GetDataSizeText(amount_total, &total);
    *progress_text = l10n_util::GetStringFUTF16(message_id, finished, total);
  }

  // device_path has to be previously created.
  void ExtractTargetedDevicePath(const ListValue& list_value, int index,
                                 FilePath* device_path) {
    Value* list_member;
    if (list_value.Get(index, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING) {
      const StringValue* string_value =
          static_cast<const StringValue*>(list_member);
      std::string image_dest;
      string_value->GetAsString(&image_dest);
      *device_path = FilePath(image_dest);
    } else {
      LOG(ERROR) << "Unable to get path string";
      device_path->clear();
    }
  }

  int64 GetDeviceSize(const std::string& device_path) {
    chromeos::disks::DiskMountManager* disk_mount_manager =
        chromeos::disks::DiskMountManager::GetInstance();
    const chromeos::disks::DiskMountManager::DiskMap& disks =
        disk_mount_manager->disks();
    return disks.find(device_path)->second->total_size_in_bytes();
  }

  bool CheckNetwork() {
    return chromeos::CrosLibrary::Get()->GetNetworkLibrary()->Connected();
  }

  FilePath zip_image_file_path_;
  GURL image_download_url_;
  std::string image_file_name_;
  content::WebContents* web_contents_;
  content::DownloadManager* download_manager_;
  content::DownloadItem*  active_download_item_;
  BurnManager* burn_manager_;
  StateMachine* state_machine_;
  bool observing_burn_lib_;
  bool working_;

  DISALLOW_COPY_AND_ASSIGN(WebUIHandler);
};

}  // namespace

}  // namespace imageburner.

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnUI
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnUI::ImageBurnUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  imageburner::WebUIHandler* handler =
      new imageburner::WebUIHandler(web_ui->GetWebContents());
  web_ui->AddMessageHandler(handler);

  Profile* profile = Profile::FromWebUI(web_ui);
  profile->GetChromeURLDataManager()->AddDataSource(
      imageburner::CreateImageburnerUIHTMLSource());
}
