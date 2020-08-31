// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/device_log_ui.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace {

class DeviceLogMessageHandler : public content::WebUIMessageHandler {
 public:
  DeviceLogMessageHandler() {}
  ~DeviceLogMessageHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "DeviceLog.getLog",
        base::BindRepeating(&DeviceLogMessageHandler::GetLog,
                            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "DeviceLog.clearLog",
        base::BindRepeating(&DeviceLogMessageHandler::ClearLog,
                            base::Unretained(this)));
  }

 private:
  void GetLog(const base::ListValue* value) const {
    base::Value data(device_event_log::GetAsString(
        device_event_log::NEWEST_FIRST, "json", "",
        device_event_log::LOG_LEVEL_DEBUG, 0));
    web_ui()->CallJavascriptFunctionUnsafe("DeviceLogUI.getLogCallback", data);
  }

  void ClearLog(const base::ListValue* value) const {
    device_event_log::ClearAll();
  }

  DISALLOW_COPY_AND_ASSIGN(DeviceLogMessageHandler);
};

}  // namespace

DeviceLogUI::DeviceLogUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<DeviceLogMessageHandler>());

  content::WebUIDataSource* html =
      content::WebUIDataSource::Create(chrome::kChromeUIDeviceLogHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"titleText", IDS_DEVICE_LOG_TITLE},
      {"autoRefreshText", IDS_DEVICE_AUTO_REFRESH},
      {"logRefreshText", IDS_DEVICE_LOG_REFRESH},
      {"logClearText", IDS_DEVICE_LOG_CLEAR},
      {"logClearTypesText", IDS_DEVICE_LOG_CLEAR_TYPES},
      {"logNoEntriesText", IDS_DEVICE_LOG_NO_ENTRIES},
      {"logLevelLabel", IDS_DEVICE_LOG_LEVEL_LABEL},
      {"logLevelErrorText", IDS_DEVICE_LOG_LEVEL_ERROR},
      {"logLevelUserText", IDS_DEVICE_LOG_LEVEL_USER},
      {"logLevelEventText", IDS_DEVICE_LOG_LEVEL_EVENT},
      {"logLevelDebugText", IDS_DEVICE_LOG_LEVEL_DEBUG},
      {"logLevelFileinfoText", IDS_DEVICE_LOG_FILEINFO},
      {"logLevelTimeDetailText", IDS_DEVICE_LOG_TIME_DETAIL},
      {"logTypeLoginText", IDS_DEVICE_LOG_TYPE_LOGIN},
      {"logTypeNetworkText", IDS_DEVICE_LOG_TYPE_NETWORK},
      {"logTypePowerText", IDS_DEVICE_LOG_TYPE_POWER},
      {"logTypeBluetoothText", IDS_DEVICE_LOG_TYPE_BLUETOOTH},
      {"logTypeUsbText", IDS_DEVICE_LOG_TYPE_USB},
      {"logTypeHidText", IDS_DEVICE_LOG_TYPE_HID},
      {"logTypePrinterText", IDS_DEVICE_LOG_TYPE_PRINTER},
      {"logTypeFidoText", IDS_DEVICE_LOG_TYPE_FIDO},
      {"logEntryFormat", IDS_DEVICE_LOG_ENTRY},
  };
  AddLocalizedStringsBulk(html, kStrings);

  html->UseStringsJs();
  html->AddResourcePath("device_log_ui.css", IDR_DEVICE_LOG_UI_CSS);
  html->AddResourcePath("device_log_ui.js", IDR_DEVICE_LOG_UI_JS);
  html->SetDefaultResource(IDR_DEVICE_LOG_UI_HTML);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html);
}

DeviceLogUI::~DeviceLogUI() {
}

}  // namespace chromeos
