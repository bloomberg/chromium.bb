// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/imageburner_ui.h"

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/download/download_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/jstemplate_builder.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

static const char kPropertyDevicePath[] = "devicePath";
static const char kPropertyFilePath[] = "filePath";
static const char kPropertyLabel[] = "label";
static const char kPropertyPath[] = "path";

static const char kConfigFileUrl[] =
    "https://dl.google.com/dl/edgedl/chromeos/recovery/recovery.conf";
static const char kImageZipFileName[] = "chromeos_image.bin.zip";
static const char kTempImageFolderName[] = "chromeos_image";
static const char kConfigFileName[] = "recovery.conf";

// Link displayed on imageburner ui.
static const std::string kMoreInfoLink =
    "http://www.chromium.org/chromium-os/chromiumos-design-docs/recovery-mode";

// Config file properties.
static const std::string kRecoveryToolVersion = "recovery_tool_version";
static const std::string kRecoveryToolLinuxVersion =
    "recovery_tool_linux_version";
static const std::string kName = "name";
static const std::string kVersion = "version";
static const std::string kDescription = "desc";
static const std::string kChannel = "channel";
static const std::string kHwid = "hwid";
static const std::string kSha1 = "sha1";
static const std::string kZipFileSize = "zipfilesize";
static const std::string kFileSize = "filesize";
static const std::string kFileName = "file";
static const std::string kUrl = "url";

// 3.9GB. It is less than 4GB because true device size ussually varies a little.
static const uint64 kMinDeviceSize =
    static_cast<uint64>(3.9) * 1000 * 1000 * 1000;

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
    localized_strings.SetString("headerTitle",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_HEADER_TITLE));
    localized_strings.SetString("headerDescription",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_HEADER_DESCRIPTION));
    localized_strings.SetString("headerLink",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_HEADER_LINK));
    localized_strings.SetString("statusDevicesNone",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_NO_DEVICES_STATUS));
    localized_strings.SetString("warningDevicesNone",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_NO_DEVICES_WARNING));
    localized_strings.SetString("statusDevicesMultiple",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_MUL_DEVICES_STATUS));
    localized_strings.SetString("statusDeviceUSB",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_USB_DEVICE_STATUS));
    localized_strings.SetString("statusDeviceSD",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_SD_DEVICE_STATUS));
    localized_strings.SetString("warningDevices",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DEVICES_WARNING));
    localized_strings.SetString("statusNoConnection",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_NO_CONNECTION_STATUS));
    localized_strings.SetString("warningNoConnection",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_NO_CONNECTION_WARNING));
    localized_strings.SetString("statusNoSpace",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_INSUFFICIENT_SPACE_STATUS));
    localized_strings.SetString("warningNoSpace",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_INSUFFICIENT_SPACE_WARNING));
    localized_strings.SetString("statusDownloading",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_DOWNLOADING_STATUS));
    localized_strings.SetString("statusUnzip",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_UNZIP_STATUS));
    localized_strings.SetString("statusBurn",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_BURN_STATUS));
    localized_strings.SetString("statusError",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_ERROR_STATUS));
    localized_strings.SetString("statusSuccess",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_SUCCESS_STATUS));
    localized_strings.SetString("warningSuccess",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_SUCCESS_DESC));
    localized_strings.SetString("title",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_PAGE_TITLE));
    localized_strings.SetString("confirmButton",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_CONFIRM_BUTTON));
    localized_strings.SetString("cancelButton",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_CANCEL_BUTTON));
    localized_strings.SetString("retryButton",
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_RETRY_BUTTON));

    localized_strings.SetString("moreInfoLink", kMoreInfoLink);

    SetFontAndTextDirection(&localized_strings);

    static const base::StringPiece imageburn_html(
        ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_IMAGEBURNER_HTML));
    const std::string full_html = jstemplate_builder::GetTemplatesHtml(
        imageburn_html, &localized_strings, "more-info-link");

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
// ImageBurnHandler
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnHandler::ImageBurnHandler(TabContents* contents)
    : tab_contents_(contents),
      download_manager_(NULL),
      download_item_observer_added_(false),
      active_download_item_(NULL),
      resource_manager_(NULL),
      state_machine_(NULL),
      observing_burn_lib_(false),
      working_(false) {
  chromeos::CrosLibrary::Get()->GetMountLibrary()->AddObserver(this);
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
      AddNetworkManagerObserver(this);
  resource_manager_ = ImageBurnResourceManager::GetInstance();
  state_machine_ = resource_manager_->state_machine();
  state_machine_->AddObserver(this);
}

ImageBurnHandler::~ImageBurnHandler() {
  chromeos::CrosLibrary::Get()->GetMountLibrary()->RemoveObserver(this);
  chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
  chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
      RemoveNetworkManagerObserver(this);
  if (active_download_item_)
    active_download_item_->RemoveObserver(this);
  if (download_manager_)
    download_manager_->RemoveObserver(this);
  if (state_machine_)
    state_machine_->RemoveObserver(this);
}

WebUIMessageHandler* ImageBurnHandler::Attach(WebUI* web_ui) {
  return WebUIMessageHandler::Attach(web_ui);
}

void ImageBurnHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getDevices",
      NewCallback(this, &ImageBurnHandler::HandleGetDevices));
  web_ui_->RegisterMessageCallback("burnImage",
      NewCallback(this, &ImageBurnHandler::HandleBurnImage));
  web_ui_->RegisterMessageCallback("cancelBurnImage",
      NewCallback(this, &ImageBurnHandler::HandleCancelBurnImage));
  web_ui_->RegisterMessageCallback("webuiInitialized",
      NewCallback(this, &ImageBurnHandler::HandleWebUIInitialized));
}

void ImageBurnHandler::DiskChanged(chromeos::MountLibraryEventType event,
                                   const chromeos::MountLibrary::Disk* disk) {
  if (!disk->is_parent() || disk->on_boot_device())
    return;
  if (event == chromeos::MOUNT_DISK_ADDED) {
    DictionaryValue disk_value;
    CreateDiskValue(*disk, &disk_value);
    web_ui_->CallJavascriptFunction("browserBridge.deviceAdded", disk_value);
  } else if (event == chromeos::MOUNT_DISK_REMOVED) {
    StringValue device_path_value(disk->device_path());
    web_ui_->CallJavascriptFunction("browserBridge.deviceRemoved",
        device_path_value);
    if (resource_manager_->target_device_path().value() ==
        disk->device_path()) {
      ProcessError(IDS_IMAGEBURN_DEVICE_NOT_FOUND_ERROR);
    }
  }
}

void ImageBurnHandler::BurnProgressUpdated(chromeos::BurnLibrary* object,
                                           chromeos::BurnEvent evt,
                                           const ImageBurnStatus& status) {
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

void ImageBurnHandler::OnNetworkManagerChanged(chromeos::NetworkLibrary* obj) {
  if (state_machine_->state() == ImageBurnStateMachine::INITIAL &&
      CheckNetwork()) {
    web_ui_->CallJavascriptFunction("browserBridge.reportNetworkDetected");
  }
  if (state_machine_->state() == ImageBurnStateMachine::DOWNLOADING &&
      !CheckNetwork()) {
    ProcessError(IDS_IMAGEBURN_NETWORK_ERROR);
  }
}

void ImageBurnHandler::OnDownloadUpdated(DownloadItem* download) {
  if (download->IsCancelled()) {
    DownloadCompleted(false);
    DCHECK(!download_item_observer_added_);
    DCHECK(!active_download_item_);
  } else if (download->IsComplete()) {
    resource_manager_->set_final_zip_file_path(download->full_path());
    DownloadCompleted(true);
    DCHECK(!download_item_observer_added_);
    DCHECK(!active_download_item_);
  } else if (download->IsPartialDownload() &&
      state_machine_->state() == ImageBurnStateMachine::DOWNLOADING) {
    base::TimeDelta remaining_time;
    download->TimeRemaining(&remaining_time);
    SendProgressSignal(DOWNLOAD, download->received_bytes(),
        download->total_bytes(), &remaining_time);
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
    if ((*it)->original_url() == image_download_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void ImageBurnHandler::OnBurnDownloadStarted(bool success) {
  if (success)
    state_machine_->OnDownloadStarted();
  else
    DownloadCompleted(false);
}

void ImageBurnHandler::OnBurnStateChanged(ImageBurnStateMachine::State state) {
  if (state == ImageBurnStateMachine::CANCELLED) {
    ProcessError(IDS_IMAGEBURN_USER_ERROR);
  } else if (state != ImageBurnStateMachine::INITIAL && !working_) {
    // User has started burn process, so let's start observing.
    HandleBurnImage(NULL);
  }
}

void ImageBurnHandler::OnError(int error_message_id) {
  StringValue error_message(l10n_util::GetStringUTF16(error_message_id));
  web_ui_->CallJavascriptFunction("browserBridge.reportFail", error_message);
  working_ = false;
}

void ImageBurnHandler::CreateDiskValue(const chromeos::MountLibrary::Disk& disk,
    DictionaryValue* disk_value) {
  string16 label = ASCIIToUTF16(disk.drive_label());
  base::i18n::AdjustStringForLocaleDirection(&label);
  disk_value->SetString(std::string(kPropertyLabel), label);
  disk_value->SetString(std::string(kPropertyFilePath), disk.file_path());
  disk_value->SetString(std::string(kPropertyDevicePath), disk.device_path());
}

void ImageBurnHandler::HandleGetDevices(const ListValue* args) {
  chromeos::MountLibrary* mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  const chromeos::MountLibrary::DiskMap& disks = mount_lib->disks();
  ListValue results_value;
  for (chromeos::MountLibrary::DiskMap::const_iterator iter =  disks.begin();
       iter != disks.end();
       ++iter) {
    chromeos::MountLibrary::Disk* disk = iter->second;
    if (disk->is_parent() && !disk->on_boot_device()) {
      DictionaryValue* disk_value = new DictionaryValue();
      CreateDiskValue(*disk, disk_value);
      results_value.Append(disk_value);
    }
  }
  web_ui_->CallJavascriptFunction("browserBridge.getDevicesCallback",
      results_value);
}

void ImageBurnHandler::HandleWebUIInitialized(const ListValue* args) {
  if (state_machine_->state() == ImageBurnStateMachine::BURNING) {
    // There is nothing else left to do but observe burn progress.
    BurnImage();
  } else if (state_machine_->state() != ImageBurnStateMachine::INITIAL) {
    // User has started burn process, so let's start observing.
    HandleBurnImage(NULL);
  }
}

void ImageBurnHandler::HandleCancelBurnImage(const ListValue* args) {
  state_machine_->OnCancelation();
}

// It may be called with NULL if there is a handler that has started burning,
// and thus set the target paths.
void ImageBurnHandler::HandleBurnImage(const ListValue* args) {
  if (args && state_machine_->new_burn_posible()) {
    if (!CheckNetwork()) {
      web_ui_->CallJavascriptFunction("browserBridge.reportNoNetwork");
      return;
    }
    FilePath target_device_path;
    ExtractTargetedDevicePath(*args, 0, &target_device_path);
    resource_manager_->set_target_device_path(target_device_path);

    FilePath target_file_path;
    ExtractTargetedDevicePath(*args, 1, &target_file_path);
    resource_manager_->set_target_file_path(target_file_path);

    uint64 device_size = GetDeviceSize(
        resource_manager_->target_device_path().value());
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
  if (resource_manager_->GetImageDir().empty()) {
    // Create image dir on File thread.
    scoped_refptr<ImageBurnTaskProxy> task = new ImageBurnTaskProxy(this);
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(task.get(), &ImageBurnTaskProxy::CreateImageDir));
  } else {
    ImageDirCreatedOnUIThread(true);
  }
}

void ImageBurnHandler::CreateImageDirOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  resource_manager_->CreateImageDir(this);
}

void ImageBurnHandler::OnImageDirCreated(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  // Transfer to UI thread.
  scoped_refptr<ImageBurnTaskProxy> task = new ImageBurnTaskProxy(this);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(), &ImageBurnTaskProxy::OnImageDirCreated,
                        success));
}

void ImageBurnHandler::ImageDirCreatedOnUIThread(bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success) {
    zip_image_file_path_ =
        resource_manager_->GetImageDir().Append(kImageZipFileName);
    resource_manager_->FetchConfigFile(tab_contents_, this);
  } else {
    DownloadCompleted(success);
  }
}

void ImageBurnHandler::OnConfigFileFetched(const ImageBurnConfigFile&
    config_file, bool success) {
  if (!success) {
    DownloadCompleted(false);
    return;
  }
  image_download_url_ = GURL(config_file.GetProperty(kUrl));
  image_file_name_ = config_file.GetProperty(kFileName);
  if (!download_manager_) {
    download_manager_ = tab_contents_->profile()->GetDownloadManager();
    download_manager_->AddObserver(this);
  }
  if (!state_machine_->download_started()) {
    resource_manager_->downloader()->AddListener(this, image_download_url_);
    if (!state_machine_->image_download_requested()) {
      state_machine_->OnImageDownloadRequested();
      resource_manager_->downloader()->DownloadFile(image_download_url_,
          zip_image_file_path_, tab_contents_);
    }
  } else if (state_machine_->download_finished()) {
    DownloadCompleted(true);
  }
}

void ImageBurnHandler::DownloadCompleted(bool success) {
  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_) {
    download_manager_->RemoveObserver(this);
    download_manager_ = NULL;
  }
  if (success) {
    state_machine_->OnDownloadFinished();
    BurnImage();
  } else {
    ProcessError(IDS_IMAGEBURN_DOWNLOAD_ERROR);
  }
}

void ImageBurnHandler::BurnImage() {
  if (!observing_burn_lib_) {
    chromeos::CrosLibrary::Get()->GetBurnLibrary()->AddObserver(this);
    observing_burn_lib_ = true;
  }
  if (state_machine_->state() == ImageBurnStateMachine::BURNING)
    return;
  state_machine_->OnBurnStarted();

  chromeos::CrosLibrary::Get()->GetBurnLibrary()->DoBurn(zip_image_file_path_,
      image_file_name_, resource_manager_->target_file_path(),
      resource_manager_->target_device_path());
}

void ImageBurnHandler::FinalizeBurn() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  state_machine_->OnSuccess();
  resource_manager_->ResetTargetPaths();
  chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
  observing_burn_lib_ = false;
  web_ui_->CallJavascriptFunction("browserBridge.reportSuccess");
  working_ = false;
}

// Error is ussually detected by all existing Burn handlers, but only first one
// that calls ProcessError should actually process it.
void ImageBurnHandler::ProcessError(int message_id) {
  // If we are in intial state, error has already been dispached.
  if (state_machine_->state() == ImageBurnStateMachine::INITIAL) {
    // We don't need burn library since we are not the ones doing the cleanup.
    if (observing_burn_lib_) {
      chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
      observing_burn_lib_ = false;
    }
    return;
  }

  // Remember burner state, since it will be reset after OnError call.
  ImageBurnStateMachine::State state = state_machine_->state();

  // Dispach error. All hadlers' OnError event will be called before returning
  // from this. This includes us, too.
  state_machine_->OnError(message_id);

  // Do cleanup.
  if (state  == ImageBurnStateMachine::DOWNLOADING) {
    if (active_download_item_) {
      // This will trigger Download canceled event. As a response to that event,
      // handlers will remove themselves as observers from download manager and
      // item
      active_download_item_->Cancel(false);
    }
  } else if (state == ImageBurnStateMachine::BURNING) {
    DCHECK(observing_burn_lib_);
    // Burn library doesn't send cancelled signal upon CancelBurnImage
    // invokation.
    chromeos::CrosLibrary::Get()->GetBurnLibrary()->CancelBurnImage();
    chromeos::CrosLibrary::Get()->GetBurnLibrary()->RemoveObserver(this);
    observing_burn_lib_ = false;
  }
  resource_manager_->ResetTargetPaths();
}

void ImageBurnHandler::SendDeviceTooSmallSignal(int64 device_size) {
  string16 size;
  GetDataSizeText(device_size, &size);
  StringValue device_size_text(size);
  web_ui_->CallJavascriptFunction("browserBridge.reportDeviceTooSmall",
      device_size_text);
}

void ImageBurnHandler::SendProgressSignal(ProgressType progress_type,
    int64 amount_finished, int64 amount_total, const base::TimeDelta* time) {
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

  web_ui_->CallJavascriptFunction("browserBridge.updateProgress", progress);
}

void ImageBurnHandler::GetProgressTimeLeftText(int message_id,
    const base::TimeDelta* time_left, string16* time_left_text) {
  if (time_left) {
    string16 time_remaining_str = TimeFormat::TimeRemaining(*time_left);
    *time_left_text =
        l10n_util::GetStringFUTF16(message_id, time_remaining_str);
  } else {
    *time_left_text = l10n_util::GetStringUTF16(message_id);
  }
}

void ImageBurnHandler::GetDataSizeText(int64 size, string16* size_text) {
  DataUnits size_units = GetByteDisplayUnits(size);
  *size_text = FormatBytes(size, size_units, true);
  base::i18n::AdjustStringForLocaleDirection(size_text);
}

void ImageBurnHandler::GetProgressText(int message_id,
    int64 amount_finished, int64 amount_total, string16* progress_text) {
  string16 finished;
  GetDataSizeText(amount_finished, &finished);
  string16 total;
  GetDataSizeText(amount_total, &total);
  *progress_text = l10n_util::GetStringFUTF16(message_id, finished, total);
}

void ImageBurnHandler::ExtractTargetedDevicePath(
    const ListValue& list_value, int index, FilePath* device_path) {
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

int64 ImageBurnHandler::GetDeviceSize(const std::string& device_path) {
  chromeos::MountLibrary* mount_lib =
      chromeos::CrosLibrary::Get()->GetMountLibrary();
  const chromeos::MountLibrary::DiskMap& disks = mount_lib->disks();
  return disks.find(device_path)->second->total_size();
}

bool ImageBurnHandler::CheckNetwork() {
  return chromeos::CrosLibrary::Get()->GetNetworkLibrary()->Connected();
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnConfigFile
//
////////////////////////////////////////////////////////////////////////////////
void ImageBurnConfigFile::reset(const std::string& file_content) {
  clear();

  std::vector<std::string> lines;
  Tokenize(file_content, "\n", &lines);

  std::vector<std::string> key_value_pair;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (lines[i].length() == 0)
      continue;
    key_value_pair.clear();
    Tokenize(lines[i], "=", &key_value_pair);
    if (key_value_pair.size() != 2)
      continue;
    if (key_value_pair[0] != kHwid) {
      config_struct_.insert(std::make_pair(key_value_pair[0],
          key_value_pair[1]));
    } else {
      hwids_.insert(key_value_pair[1]);
    }
  }
}

void ImageBurnConfigFile::clear() {
  config_struct_.clear();
  hwids_.clear();
}

const std::string& ImageBurnConfigFile::GetProperty(
    const std::string& property_name)
    const {
  std::map<std::string, std::string>::const_iterator property =
      config_struct_.find(property_name);
  if (property != config_struct_.end())
    return property->second;
  else
    return EmptyString();
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnStateMachine
//
////////////////////////////////////////////////////////////////////////////////
ImageBurnStateMachine::ImageBurnStateMachine()
    : image_download_requested_(false),
      download_started_(false),
      download_finished_(false),
      state_(INITIAL) {
}

void ImageBurnStateMachine::OnError(int error_message_id) {
  if (state_ == INITIAL)
    return;
  if (!download_finished_) {
    download_started_ = false;
    image_download_requested_ = false;
  }
  state_ = INITIAL;
  FOR_EACH_OBSERVER(Observer, observers_, OnError(error_message_id));
}

void ImageBurnStateMachine::OnSuccess() {
  if (state_ == INITIAL)
    return;
  state_ = INITIAL;
  OnStateChanged();
}

void ImageBurnStateMachine::OnCancelation() {
  // We use state CANCELLED only to let observers know that they have to
  // process cancelation. We don't actually change the state.
  FOR_EACH_OBSERVER(Observer, observers_, OnBurnStateChanged(CANCELLED));
}

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnResourceManagerTaskProxy
//
////////////////////////////////////////////////////////////////////////////////

class ImageBurnResourceManagerTaskProxy
    : public base::RefCountedThreadSafe<ImageBurnResourceManagerTaskProxy> {
 public:
  ImageBurnResourceManagerTaskProxy() {}

  void OnConfigFileDownloaded() {
    ImageBurnResourceManager::GetInstance()->
        OnConfigFileDownloadedOnFileThread();
  }

  void ConfigFileFetched(bool success, std::string content) {
    ImageBurnResourceManager::GetInstance()->
        ConfigFileFetchedOnUIThread(success, content);
  }

 private:
  ~ImageBurnResourceManagerTaskProxy() {}

  friend class base::RefCountedThreadSafe<ImageBurnResourceManagerTaskProxy>;

  DISALLOW_COPY_AND_ASSIGN(ImageBurnResourceManagerTaskProxy);
};

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnResourceManager
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnResourceManager::ImageBurnResourceManager()
    : download_manager_(NULL),
      download_item_observer_added_(false),
      active_download_item_(NULL),
      config_file_url_(kConfigFileUrl),
      config_file_requested_(false),
      config_file_fetched_(false),
      state_machine_(new ImageBurnStateMachine()),
      downloader_(NULL) {
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
    ConfigFileFetchedOnUIThread(false, "");
    DCHECK(!download_item_observer_added_);
    DCHECK(active_download_item_ == NULL);
  } else if (download->IsComplete()) {
    scoped_refptr<ImageBurnResourceManagerTaskProxy> task =
        new ImageBurnResourceManagerTaskProxy();
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        NewRunnableMethod(task.get(),
            &ImageBurnResourceManagerTaskProxy::OnConfigFileDownloaded));
  }
}

void ImageBurnResourceManager::OnConfigFileDownloadedOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  std::string config_file_content;
  bool success =
      file_util::ReadFileToString(config_file_path_, &config_file_content);
  scoped_refptr<ImageBurnResourceManagerTaskProxy> task =
      new ImageBurnResourceManagerTaskProxy();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(task.get(),
          &ImageBurnResourceManagerTaskProxy::ConfigFileFetched, success,
          config_file_content));
}

void ImageBurnResourceManager::ModelChanged() {
  std::vector<DownloadItem*> downloads;
  download_manager_->GetTemporaryDownloads(GetImageDir(), &downloads);
  if (download_item_observer_added_)
    return;
  for (std::vector<DownloadItem*>::const_iterator it = downloads.begin();
      it != downloads.end();
      ++it) {
    if ((*it)->GetURL() == config_file_url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
      active_download_item_ = *it;
      break;
    }
  }
}

void ImageBurnResourceManager::OnBurnDownloadStarted(bool success) {
  if (!success)
    ConfigFileFetchedOnUIThread(false, "");
}

void ImageBurnResourceManager::CreateImageDir(Delegate* delegate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  bool success = true;
  if (image_dir_.empty()) {
    CHECK(PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &image_dir_));
    image_dir_ = image_dir_.Append(kTempImageFolderName);
    success = file_util::CreateDirectory(image_dir_);
  }
  delegate->OnImageDirCreated(success);
}

const FilePath& ImageBurnResourceManager::GetImageDir() {
  return image_dir_;
}

void ImageBurnResourceManager::FetchConfigFile(TabContents* tab_contents,
    Delegate* delegate) {
  if (config_file_fetched_) {
    delegate->OnConfigFileFetched(config_file_, true);
    return;
  }
  downloaders_.push_back(delegate->AsWeakPtr());

  if (config_file_requested_)
    return;
  config_file_requested_ = true;

  config_file_path_ = GetImageDir().Append(kConfigFileName);
  download_manager_ = tab_contents->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);
  downloader()->AddListener(this, config_file_url_);
  downloader()->DownloadFile(config_file_url_, config_file_path_, tab_contents);
}

void ImageBurnResourceManager::ConfigFileFetchedOnUIThread(bool fetched,
    const std::string& content) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (config_file_fetched_)
    return;

  if (active_download_item_) {
    active_download_item_->RemoveObserver(this);
    active_download_item_ = NULL;
  }
  download_item_observer_added_ = false;
  if (download_manager_)
    download_manager_->RemoveObserver(this);

  config_file_fetched_ = fetched;

  if (fetched) {
    config_file_.reset(content);
  } else {
    config_file_.clear();
  }

  for (size_t i = 0; i < downloaders_.size(); ++i) {
    if (downloaders_[i]) {
      downloaders_[i]->OnConfigFileFetched(config_file_, fetched);
    }
  }
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
    ImageBurnResourceManager::GetInstance()->downloader()->
        CreateFileStreamOnFileThread(url, target_path, tab_contents);
  }

  void OnFileStreamCreated(const GURL& url,
                           const FilePath& file_path,
                           TabContents* tab_contents,
                           net::FileStream* created_file_stream) {
    ImageBurnResourceManager::GetInstance()->downloader()->
        OnFileStreamCreatedOnUIThread(url, file_path, tab_contents,
        created_file_stream);
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
ImageBurnDownloader::ImageBurnDownloader() {}

ImageBurnDownloader::~ImageBurnDownloader() {}

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
    TabContents* tab_contents) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  DCHECK(!file_path.empty());

  scoped_ptr<net::FileStream> file_stream(new net::FileStream);
  if (file_stream->Open(file_path, base::PLATFORM_FILE_CREATE_ALWAYS |
                        base::PLATFORM_FILE_WRITE))
    file_stream.reset(NULL);

  scoped_refptr<ImageBurnDownloaderTaskProxy> task =
      new ImageBurnDownloaderTaskProxy();
  // Call callback method on UI thread.
  BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(task.get(),
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
  listeners_.insert(std::make_pair(url, listener->AsWeakPtr()));
}

void ImageBurnDownloader::DownloadStarted(bool success, const GURL& url) {
  std::pair<ListenerMap::iterator, ListenerMap::iterator> listener_range =
      listeners_.equal_range(url);
  for (ListenerMap::iterator current_listener = listener_range.first;
       current_listener != listener_range.second;
       ++current_listener) {
    if (current_listener->second)
      current_listener->second->OnBurnDownloadStarted(success);
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
