// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/about_page_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/platform_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "webkit/glue/webkit_glue.h"

#if defined(CHROME_V8)
#include "v8/include/v8.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/update_library.h"
#endif

namespace {

// These are used as placeholder text around the links in the text in the
// license.
const wchar_t kBeginLink[] = L"BEGIN_LINK";
const wchar_t kEndLink[] = L"END_LINK";
const wchar_t kBeginLinkChr[] = L"BEGIN_LINK_CHR";
const wchar_t kBeginLinkOss[] = L"BEGIN_LINK_OSS";
const wchar_t kEndLinkChr[] = L"END_LINK_CHR";
const wchar_t kEndLinkOss[] = L"END_LINK_OSS";

// Returns a substring [start, end) from |text|.
std::wstring StringSubRange(const std::wstring& text, size_t start,
                            size_t end) {
  DCHECK(end > start);
  return text.substr(start, end - start);
}

struct LocalizeEntry {
  const char* identifier;
  int resource;
};

const LocalizeEntry localize_table[] = {
#if defined (OS_CHROMEOS)
    { "product", IDS_PRODUCT_OS_NAME },
    { "os", IDS_PRODUCT_OS_NAME },
    { "loading", IDS_ABOUT_PAGE_LOADING },
    { "check_now", IDS_ABOUT_PAGE_CHECK_NOW },
    { "update_status", IDS_UPGRADE_CHECK_STARTED },
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
    { "user_agent", IDS_ABOUT_VERSION_USER_AGENT },
    { "command_line", IDS_ABOUT_VERSION_COMMAND_LINE },
    { "aboutPage", IDS_ABOUT_PAGE_TITLE }
};

void LocalizedStrings(DictionaryValue* localized_strings) {
  for (size_t n = 0; n != arraysize(localize_table); ++n) {
    localized_strings->SetString(localize_table[n].identifier,
        l10n_util::GetStringUTF16(localize_table[n].resource));
  }
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

  LocalizedStrings(localized_strings);

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

  std::wstring text = l10n_util::GetString(IDS_ABOUT_VERSION_LICENSE);

  bool chromium_url_appears_first =
      text.find(kBeginLinkChr) < text.find(kBeginLinkOss);

  size_t link1 = text.find(kBeginLink);
  DCHECK(link1 != std::wstring::npos);
  size_t link1_end = text.find(kEndLink, link1);
  DCHECK(link1_end != std::wstring::npos);
  size_t link2 = text.find(kBeginLink, link1_end);
  DCHECK(link2 != std::wstring::npos);
  size_t link2_end = text.find(kEndLink, link2);
  DCHECK(link1_end != std::wstring::npos);

  localized_strings->SetString("license_content_0",
      WideToUTF16Hack(text.substr(0, link1)));
  localized_strings->SetString("license_content_1",
      WideToUTF16Hack(StringSubRange(text, link1_end + wcslen(kEndLinkOss),
                                     link2)));
  localized_strings->SetString("license_content_2",
      WideToUTF16Hack(text.substr(link2_end + wcslen(kEndLinkOss))));

  // The Chromium link within the main text of the dialog.
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_content_0" : "license_link_content_1",
      WideToUTF16Hack(StringSubRange(text,
                      text.find(kBeginLinkChr) + wcslen(kBeginLinkChr),
                      text.find(kEndLinkChr))));
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_0" : "license_link_1",
      l10n_util::GetStringUTF16(IDS_CHROMIUM_PROJECT_URL));

  // The Open Source link within the main text of the dialog.
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_content_1" : "license_link_content_0",
      WideToUTF16Hack(StringSubRange(text,
          text.find(kBeginLinkOss) + wcslen(kBeginLinkOss),
          text.find(kEndLinkOss))));
  localized_strings->SetString(chromium_url_appears_first ?
      "license_link_1" : "license_link_0", chrome::kAboutCreditsURL);

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
  dom_ui_->RegisterMessageCallback("PageReady",
      NewCallback(this, &AboutPageHandler::PageReady));

#if defined(OS_CHROMEOS)
  dom_ui_->RegisterMessageCallback("CheckNow",
      NewCallback(this, &AboutPageHandler::CheckNow));
#endif
}

void AboutPageHandler::PageReady(const ListValue* args) {
#if defined(OS_CHROMEOS)
  // Version information is loaded from a callback
  loader_.GetVersion(&consumer_,
                     NewCallback(this, &AboutPageHandler::OnOSVersion));

  update_observer_.reset(new UpdateObserver(this));
  chromeos::CrosLibrary::Get()->GetUpdateLibrary()->
      AddObserver(update_observer_.get());

  CheckNow(NULL);
#endif
}

#if defined(OS_CHROMEOS)

void AboutPageHandler::CheckNow(const ListValue* args) {
  if (chromeos::InitiateUpdateCheck)
    chromeos::InitiateUpdateCheck();
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
        // TODO(viettrungluu): need UTF-16 convenience form to eliminate hack.
        message = WideToUTF16Hack(
            l10n_util::GetStringF(IDS_UPDATE_DOWNLOADING, progress_));
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
    scoped_ptr<Value> version_string(Value::CreateStringValue(message));
    dom_ui_->CallJavascriptFunction(L"AboutPage.updateStatusCallback",
                                    *version_string);

    scoped_ptr<Value> enabled_value(Value::CreateBooleanValue(enabled));
    dom_ui_->CallJavascriptFunction(L"AboutPage.updateEnableCallback",
                                    *enabled_value);

    scoped_ptr<Value> image_string(Value::CreateStringValue(image));
    dom_ui_->CallJavascriptFunction(L"AboutPage.setUpdateImage",
                                    *image_string);
  }
}

void AboutPageHandler::OnOSVersion(chromeos::VersionLoader::Handle handle,
                                   std::string version) {
  if (version.size()) {
    scoped_ptr<Value> version_string(Value::CreateStringValue(version));
    dom_ui_->CallJavascriptFunction(L"AboutPage.updateOSVersionCallback",
                                    *version_string);
  }
}
#endif

