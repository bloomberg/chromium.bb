// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/about_page_handler.h"

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webkit_glue.h"

#if defined(CHROME_V8)
#include "v8/include/v8.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#endif

namespace {

// These are used as placeholder text around the links in the text in the
// license.
const char kBeginLink[] = "BEGIN_LINK";
const char kEndLink[] = "END_LINK";
const char kBeginLinkChr[] = "BEGIN_LINK_CHR";
const char kBeginLinkOss[] = "BEGIN_LINK_OSS";
const char kEndLinkChr[] = "END_LINK_CHR";
const char kEndLinkOss[] = "END_LINK_OSS";
#if defined(OS_CHROMEOS)
const char kBeginLinkCrosOss[] = "BEGIN_LINK_CROS_OSS";
const char kEndLinkCrosOss[] = "END_LINK_CROS_OSS";
#endif

// Returns a substring [start, end) from |text|.
std::string StringSubRange(const std::string& text, size_t start,
                           size_t end) {
  DCHECK(end > start);
  return text.substr(start, end - start);
}

}  // namespace

#if defined(OS_CHROMEOS)

class AboutPageHandler::UpdateObserver
    : public chromeos::UpdateLibrary::Observer {
 public:
  explicit UpdateObserver(AboutPageHandler* handler) : page_handler_(handler) {}
  virtual ~UpdateObserver() {}

 private:
  virtual void UpdateStatusChanged(chromeos::UpdateLibrary* object) {
    page_handler_->UpdateStatus(object->status());
  }

  AboutPageHandler* page_handler_;

  DISALLOW_COPY_AND_ASSIGN(UpdateObserver);
};

#endif

AboutPageHandler::AboutPageHandler()
#if defined(OS_CHROMEOS)
    : progress_(-1),
      sticky_(false),
      started_(false)
#endif
{}

AboutPageHandler::~AboutPageHandler() {
#if defined(OS_CHROMEOS)
  if (update_observer_.get()) {
    chromeos::CrosLibrary::Get()->GetUpdateLibrary()->
        RemoveObserver(update_observer_.get());
  }
#endif
}

void AboutPageHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
#if defined (OS_CHROMEOS)
    { "product", IDS_PRODUCT_OS_NAME },
    { "os", IDS_PRODUCT_OS_NAME },
    { "loading", IDS_ABOUT_PAGE_LOADING },
    { "check_now", IDS_ABOUT_PAGE_CHECK_NOW },
    { "update_status", IDS_UPGRADE_CHECK_STARTED },
    { "restart_now", IDS_RELAUNCH_AND_UPDATE },
#else
    { "product", IDS_PRODUCT_NAME },
    { "check_now", IDS_ABOUT_CHROME_UPDATE_CHECK },
#endif
    { "browser", IDS_PRODUCT_NAME },
    { "more_info", IDS_ABOUT_PAGE_MORE_INFO },
    { "copyright", IDS_ABOUT_VERSION_COPYRIGHT },
    { "channel", IDS_ABOUT_PAGE_CHANNEL },
    { "release", IDS_ABOUT_PAGE_CHANNEL_RELEASE },
    { "beta", IDS_ABOUT_PAGE_CHANNEL_BETA },
    { "development", IDS_ABOUT_PAGE_CHANNEL_DEVELOPMENT },
    { "canary", IDS_ABOUT_PAGE_CHANNEL_CANARY },
    { "channel_warning_header", IDS_ABOUT_PAGE_CHANNEL_WARNING_HEADER },
    { "channel_warning_text", IDS_ABOUT_PAGE_CHANNEL_WARNING_TEXT },
    { "user_agent", IDS_ABOUT_VERSION_USER_AGENT },
    { "command_line", IDS_ABOUT_VERSION_COMMAND_LINE },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "aboutPage", IDS_ABOUT_TAB_TITLE);

  // browser version

  chrome::VersionInfo version_info;
  DCHECK(version_info.is_valid());

  std::string browser_version = version_info.Version();
  std::string version_modifier = platform_util::GetVersionStringModifier();
  if (!version_modifier.empty())
    browser_version += " " + version_modifier;

#if !defined(GOOGLE_CHROME_BUILD)
  browser_version += " (";
  browser_version += version_info.LastChange();
  browser_version += ")";
#endif

  localized_strings->SetString("browser_version", browser_version);

  // license

  std::string text = l10n_util::GetStringUTF8(IDS_ABOUT_VERSION_LICENSE);

  bool chromium_url_appears_first =
      text.find(kBeginLinkChr) < text.find(kBeginLinkOss);

  size_t link1 = text.find(kBeginLink);
  DCHECK(link1 != std::string::npos);
  size_t link1_end = text.find(kEndLink, link1);
  DCHECK(link1_end != std::string::npos);
  size_t link2 = text.find(kBeginLink, link1_end);
  DCHECK(link2 != std::string::npos);
  size_t link2_end = text.find(kEndLink, link2);
  DCHECK(link2_end != std::string::npos);

  localized_strings->SetString("license_content_0", text.substr(0, link1));
  localized_strings->SetString("license_content_1",
      StringSubRange(text, link1_end + strlen(kEndLinkOss), link2));
  localized_strings->SetString("license_content_2",
      text.substr(link2_end + strlen(kEndLinkOss)));

  // The Chromium link within the main text of the dialog.
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_content_0" : "license_link_content_1",
      StringSubRange(text,
                     text.find(kBeginLinkChr) + strlen(kBeginLinkChr),
                     text.find(kEndLinkChr)));
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kChromiumProjectURL));
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_0" : "license_link_1", url.spec());

  // The Open Source link within the main text of the dialog.  We need to use
  // the chrome:// variant instead of about:credits; the latter will get
  // rewritten to about:blank.
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_content_1" : "license_link_content_0",
      StringSubRange(text,
                     text.find(kBeginLinkOss) + strlen(kBeginLinkOss),
                     text.find(kEndLinkOss)));
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_1" : "license_link_0", chrome::kChromeUIAboutCreditsURL);

#if defined(OS_CHROMEOS)
  std::string cros_text =
      l10n_util::GetStringUTF8(IDS_ABOUT_CROS_VERSION_LICENSE);

  size_t cros_link = cros_text.find(kBeginLinkCrosOss);
  DCHECK(cros_link != std::string::npos);
  size_t cros_link_end = cros_text.find(kEndLinkCrosOss, cros_link);
  DCHECK(cros_link_end != std::string::npos);

  localized_strings->SetString("cros_license_content_0",
      cros_text.substr(0, cros_link));
  localized_strings->SetString("cros_license_content_1",
      cros_text.substr(cros_link_end + strlen(kEndLinkCrosOss)));
  localized_strings->SetString("cros_license_link_content_0",
      StringSubRange(cros_text, cros_link + strlen(kBeginLinkCrosOss),
                     cros_link_end));
  localized_strings->SetString("cros_license_link_0",
      chrome::kChromeUIAboutOSCreditsURL);
#endif

  // webkit

  localized_strings->SetString("webkit_version",
                               webkit_glue::GetWebKitVersion());

  // javascript

#if defined(CHROME_V8)
  localized_strings->SetString("js_engine", "V8");
  localized_strings->SetString("js_engine_version", v8::V8::GetVersion());
#else
  localized_strings->SetString("js_engine", "JavaScriptCore");
  localized_strings->SetString("js_engine_version",
                               webkit_glue::GetWebKitVersion());
#endif

  // user agent

  localized_strings->SetString("user_agent_info",
                               webkit_glue::GetUserAgent(GURL()));

  // command line

#if defined(OS_WIN)
  localized_strings->SetString("command_line_info",
      WideToUTF16(CommandLine::ForCurrentProcess()->command_line_string()));
#elif defined(OS_POSIX)
  // TODO(viettrungluu): something horrible might happen if there are non-UTF-8
  // arguments (since |SetString()| requires Unicode).
  std::string command_line = "";
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  localized_strings->SetString("command_line_info", command_line);
#endif
}

void AboutPageHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("PageReady",
      NewCallback(this, &AboutPageHandler::PageReady));
  web_ui_->RegisterMessageCallback("SetReleaseTrack",
      NewCallback(this, &AboutPageHandler::SetReleaseTrack));

#if defined(OS_CHROMEOS)
  web_ui_->RegisterMessageCallback("CheckNow",
      NewCallback(this, &AboutPageHandler::CheckNow));
  web_ui_->RegisterMessageCallback("RestartNow",
      NewCallback(this, &AboutPageHandler::RestartNow));
#endif
}

void AboutPageHandler::PageReady(const ListValue* args) {
#if defined(OS_CHROMEOS)
  // Version information is loaded from a callback
  loader_.GetVersion(&consumer_,
                     NewCallback(this, &AboutPageHandler::OnOSVersion),
                     chromeos::VersionLoader::VERSION_FULL);

  chromeos::UpdateLibrary* update_library =
      chromeos::CrosLibrary::Get()->GetUpdateLibrary();

  // Update the channel information.
  std::string channel = update_library->GetReleaseTrack();
  scoped_ptr<Value> channel_string(Value::CreateStringValue(channel));
  web_ui_->CallJavascriptFunction("AboutPage.updateSelectedOptionCallback",
                                  *channel_string);

  update_observer_.reset(new UpdateObserver(this));
  update_library->AddObserver(update_observer_.get());

  // Update the WebUI page with the current status. See comments below.
  UpdateStatus(update_library->status());

  // Initiate update check. UpdateStatus() below will be called when we
  // get update status via update_observer_. If the update has been
  // already complete, update_observer_ won't receive a notification.
  // This is why we manually update the WebUI page above.
  CheckNow(NULL);
#endif
}

void AboutPageHandler::SetReleaseTrack(const ListValue* args) {
#if defined(OS_CHROMEOS)
  if (!chromeos::UserManager::Get()->current_user_is_owner()) {
    LOG(WARNING) << "Non-owner tried to change release track.";
    return;
  }
  const std::string channel = UTF16ToUTF8(ExtractStringValue(args));
  chromeos::CrosLibrary::Get()->GetUpdateLibrary()->SetReleaseTrack(channel);
#endif
}

#if defined(OS_CHROMEOS)

void AboutPageHandler::CheckNow(const ListValue* args) {
  // Make sure that libcros is loaded and OOBE is complete.
  if (chromeos::CrosLibrary::Get()->EnsureLoaded() &&
      (!WizardController::default_controller() ||
        WizardController::IsDeviceRegistered())) {
    chromeos::CrosLibrary::Get()->GetUpdateLibrary()->CheckForUpdate();
  }
}

void AboutPageHandler::RestartNow(const ListValue* args) {
  chromeos::CrosLibrary::Get()->GetPowerLibrary()->RequestRestart();
}

void AboutPageHandler::UpdateStatus(
    const chromeos::UpdateLibrary::Status& status) {
  string16 message;
  std::string image = "up-to-date";
  bool enabled = false;

  switch (status.status) {
    case chromeos::UPDATE_STATUS_IDLE:
      if (!sticky_) {
        message = l10n_util::GetStringFUTF16(IDS_UPGRADE_ALREADY_UP_TO_DATE,
            l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME));
        enabled = true;
      }
      break;
    case chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE:
      message = l10n_util::GetStringUTF16(IDS_UPGRADE_CHECK_STARTED);
      sticky_ = false;
      break;
    case chromeos::UPDATE_STATUS_UPDATE_AVAILABLE:
      message = l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE);
      started_ = true;
      break;
    case chromeos::UPDATE_STATUS_DOWNLOADING:
    {
      int progress = static_cast<int>(status.download_progress * 100.0);
      if (progress != progress_) {
        progress_ = progress;
        message = l10n_util::GetStringFUTF16Int(IDS_UPDATE_DOWNLOADING,
                                                progress_);
      }
      started_ = true;
    }
      break;
    case chromeos::UPDATE_STATUS_VERIFYING:
      message = l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING);
      started_ = true;
      break;
    case chromeos::UPDATE_STATUS_FINALIZING:
      message = l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING);
      started_ = true;
      break;
    case chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT:
      message = l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED);
      image = "available";
      sticky_ = true;
      break;
    default:
    // case UPDATE_STATUS_ERROR:
    // case UPDATE_STATUS_REPORTING_ERROR_EVENT:

    // The error is only displayed if we were able to determine an
    // update was available.
      if (started_) {
        message = l10n_util::GetStringUTF16(IDS_UPDATE_ERROR);
        image = "fail";
        enabled = true;
        sticky_ = true;
        started_ = false;
      }
      break;
  }
  if (message.size()) {
    scoped_ptr<Value> update_message(Value::CreateStringValue(message));
    // "Checking for update..." needs to be shown for a while, so users
    // can read it, hence insert delay for this.
    scoped_ptr<Value> insert_delay(Value::CreateBooleanValue(
        status.status == chromeos::UPDATE_STATUS_CHECKING_FOR_UPDATE));
    web_ui_->CallJavascriptFunction("AboutPage.updateStatusCallback",
                                    *update_message, *insert_delay);

    scoped_ptr<Value> enabled_value(Value::CreateBooleanValue(enabled));
    web_ui_->CallJavascriptFunction("AboutPage.updateEnableCallback",
                                    *enabled_value);

    scoped_ptr<Value> image_string(Value::CreateStringValue(image));
    web_ui_->CallJavascriptFunction("AboutPage.setUpdateImage",
                                    *image_string);
  }
  // We'll change the "Check For Update" button to "Restart" button.
  if (status.status == chromeos::UPDATE_STATUS_UPDATED_NEED_REBOOT) {
    web_ui_->CallJavascriptFunction("AboutPage.changeToRestartButton");
  }
}

void AboutPageHandler::OnOSVersion(chromeos::VersionLoader::Handle handle,
                                   std::string version) {
  if (version.size()) {
    scoped_ptr<Value> version_string(Value::CreateStringValue(version));
    web_ui_->CallJavascriptFunction("AboutPage.updateOSVersionCallback",
                                    *version_string);
  }
}
#endif
