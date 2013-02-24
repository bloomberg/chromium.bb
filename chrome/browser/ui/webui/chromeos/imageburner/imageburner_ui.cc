// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/imageburner/imageburner_ui.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/imageburner/burn_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/time_format.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {
namespace imageburner {

namespace {

const char kPropertyDevicePath[] = "devicePath";
const char kPropertyFilePath[] = "filePath";
const char kPropertyLabel[] = "label";
const char kPropertyPath[] = "path";
const char kPropertyDeviceType[] = "type";

// Link displayed on imageburner ui.
const char kMoreInfoLink[] =
    "http://www.chromium.org/chromium-os/chromiumos-design-docs/recovery-mode";

content::WebUIDataSource* CreateImageburnerUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIImageBurnerHost);

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

  source->SetJsonPath("strings.js");
  source->AddResourcePath("image_burner.js", IDR_IMAGEBURNER_JS);
  source->SetDefaultResource(IDR_IMAGEBURNER_HTML);
  return source;
}

class WebUIHandler
    : public content::WebUIMessageHandler,
      public BurnController::Delegate {
 public:
  explicit WebUIHandler(content::WebContents* contents)
      : burn_controller_(BurnController::CreateBurnController(contents, this)){
  }

  virtual ~WebUIHandler() {
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

  // BurnController::Delegate override.
  virtual void OnSuccess() OVERRIDE {
    web_ui()->CallJavascriptFunction("browserBridge.reportSuccess");
  }

  // BurnController::Delegate override.
  virtual void OnFail(int error_message_id) OVERRIDE {
    StringValue error_message(l10n_util::GetStringUTF16(error_message_id));
    web_ui()->CallJavascriptFunction("browserBridge.reportFail", error_message);
  }

  // BurnController::Delegate override.
  virtual void OnDeviceAdded(const disks::DiskMountManager::Disk& disk)
      OVERRIDE {
    DictionaryValue disk_value;
    CreateDiskValue(disk, &disk_value);
    web_ui()->CallJavascriptFunction("browserBridge.deviceAdded", disk_value);
  }

  // BurnController::Delegate override.
  virtual void OnDeviceRemoved(const disks::DiskMountManager::Disk& disk)
      OVERRIDE {
    StringValue device_path_value(disk.device_path());
    web_ui()->CallJavascriptFunction("browserBridge.deviceRemoved",
                                     device_path_value);
  }

  // BurnController::Delegate override.
  virtual void OnDeviceTooSmall(int64 device_size) OVERRIDE {
    string16 size;
    GetDataSizeText(device_size, &size);
    StringValue device_size_text(size);
    web_ui()->CallJavascriptFunction("browserBridge.reportDeviceTooSmall",
                                     device_size_text);
  }

  // BurnController::Delegate override.
  virtual void OnProgress(ProgressType progress_type,
                          int64 amount_finished,
                          int64 amount_total) OVERRIDE {
    const string16 time_remaining_text =
        l10n_util::GetStringUTF16(IDS_IMAGEBURN_PROGRESS_TIME_UNKNOWN);
    SendProgressSignal(progress_type, amount_finished, amount_total,
                       time_remaining_text);
  }

  // BurnController::Delegate override.
  virtual void OnProgressWithRemainingTime(
      ProgressType progress_type,
      int64 amount_finished,
      int64 amount_total,
      const base::TimeDelta& time_remaining) OVERRIDE {
    const string16 time_remaining_text = l10n_util::GetStringFUTF16(
        IDS_IMAGEBURN_DOWNLOAD_TIME_REMAINING,
        TimeFormat::TimeRemaining(time_remaining));
    SendProgressSignal(progress_type, amount_finished, amount_total,
                       time_remaining_text);
  }

  // BurnController::Delegate override.
  virtual void OnNetworkDetected() OVERRIDE {
    web_ui()->CallJavascriptFunction("browserBridge.reportNetworkDetected");
  }

  // BurnController::Delegate override.
  virtual void OnNoNetwork() OVERRIDE {
    web_ui()->CallJavascriptFunction("browserBridge.reportNoNetwork");
  }

 private:
  void CreateDiskValue(const disks::DiskMountManager::Disk& disk,
                       DictionaryValue* disk_value) {
    string16 label = ASCIIToUTF16(disk.drive_label());
    base::i18n::AdjustStringForLocaleDirection(&label);
    disk_value->SetString(std::string(kPropertyLabel), label);
    disk_value->SetString(std::string(kPropertyFilePath), disk.file_path());
    disk_value->SetString(std::string(kPropertyDevicePath), disk.device_path());
    disk_value->SetString(std::string(kPropertyDeviceType),
        disks::DiskMountManager::DeviceTypeToString(disk.device_type()));
  }

  // Callback for the "getDevices" message.
  void HandleGetDevices(const ListValue* args) {
    const std::vector<disks::DiskMountManager::Disk> disks
        = burn_controller_->GetBurnableDevices();
    ListValue results_value;
    for (size_t i = 0; i != disks.size(); ++i) {
      DictionaryValue* disk_value = new DictionaryValue();
      CreateDiskValue(disks[i], disk_value);
      results_value.Append(disk_value);
    }
    web_ui()->CallJavascriptFunction("browserBridge.getDevicesCallback",
                                     results_value);
  }

  // Callback for the webuiInitialized message.
  void HandleWebUIInitialized(const ListValue* args) {
    burn_controller_->Init();
  }

  // Callback for the "cancelBurnImage" message.
  void HandleCancelBurnImage(const ListValue* args) {
    burn_controller_->CancelBurnImage();
  }

  // Callback for the "burnImage" message.
  // It may be called with NULL if there is a handler that has started burning,
  // and thus set the target paths.
  void HandleBurnImage(const ListValue* args) {
    base::FilePath target_device_path;
    ExtractTargetedDevicePath(*args, 0, &target_device_path);

    base::FilePath target_file_path;
    ExtractTargetedDevicePath(*args, 1, &target_file_path);

    burn_controller_->StartBurnImage(target_device_path, target_file_path);
  }

  // Reports update to UI
  void SendProgressSignal(ProgressType progress_type,
                          int64 amount_finished,
                          int64 amount_total,
                          const string16& time_remaining_text) {
    DictionaryValue progress;
    int progress_message_id = 0;
    switch (progress_type) {
      case DOWNLOADING:
        progress.SetString("progressType", "download");
        progress_message_id = IDS_IMAGEBURN_DOWNLOAD_PROGRESS_TEXT;
        break;
      case UNZIPPING:
        progress.SetString("progressType", "unzip");
        break;
      case BURNING:
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
    progress.SetString("timeLeftText", time_remaining_text);

    web_ui()->CallJavascriptFunction("browserBridge.updateProgress", progress);
  }

  // size_text should be previously created.
  void GetDataSizeText(int64 size, string16* size_text) {
    *size_text = ui::FormatBytes(size);
    base::i18n::AdjustStringForLocaleDirection(size_text);
  }

  // progress_text should be previously created.
  void GetProgressText(int message_id,
                       int64 amount_finished,
                       int64 amount_total,
                       string16* progress_text) {
    string16 finished;
    GetDataSizeText(amount_finished, &finished);
    string16 total;
    GetDataSizeText(amount_total, &total);
    *progress_text = l10n_util::GetStringFUTF16(message_id, finished, total);
  }

  // device_path has to be previously created.
  void ExtractTargetedDevicePath(const ListValue& list_value,
                                 int index,
                                 base::FilePath* device_path) {
    const Value* list_member;
    std::string image_dest;
    if (list_value.Get(index, &list_member) &&
        list_member->GetType() == Value::TYPE_STRING &&
        list_member->GetAsString(&image_dest)) {
      *device_path = base::FilePath(image_dest);
    } else {
      LOG(ERROR) << "Unable to get path string";
      device_path->clear();
    }
  }

  scoped_ptr<BurnController> burn_controller_;

  DISALLOW_COPY_AND_ASSIGN(WebUIHandler);
};

}  // namespace

}  // namespace imageburner
}  // namespace chromeos

////////////////////////////////////////////////////////////////////////////////
//
// ImageBurnUI
//
////////////////////////////////////////////////////////////////////////////////

ImageBurnUI::ImageBurnUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  chromeos::imageburner::WebUIHandler* handler =
      new chromeos::imageburner::WebUIHandler(web_ui->GetWebContents());
  web_ui->AddMessageHandler(handler);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, chromeos::imageburner::CreateImageburnerUIHTMLSource());
}
