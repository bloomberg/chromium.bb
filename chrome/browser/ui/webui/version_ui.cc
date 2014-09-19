// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_ui.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/google_chrome_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "grit/browser_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "v8/include/v8.h"

#if defined(ENABLE_THEMES)
#include "chrome/browser/ui/webui/theme_source.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "chrome/browser/ui/android/android_about_app_info.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/version_handler_chromeos.h"
#endif

namespace {

content::WebUIDataSource* CreateVersionUIDataSource(Profile* profile) {
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIVersionHost);

  // Localized and data strings.
  html_source->AddLocalizedString("title", IDS_ABOUT_VERSION_TITLE);
  html_source->AddLocalizedString("application_label", IDS_PRODUCT_NAME);
  chrome::VersionInfo version_info;
  html_source->AddString("version", version_info.Version());
  html_source->AddString("version_modifier",
                         chrome::VersionInfo::GetVersionStringModifier());
  html_source->AddLocalizedString("os_name", IDS_ABOUT_VERSION_OS);
  html_source->AddLocalizedString("platform", IDS_PLATFORM_LABEL);
  html_source->AddString("os_type", version_info.OSType());
  html_source->AddString("blink_version", content::GetWebKitVersion());
  html_source->AddString("js_engine", "V8");
  html_source->AddString("js_version", v8::V8::GetVersion());

#if defined(OS_ANDROID)
  html_source->AddString("os_version", AndroidAboutAppInfo::GetOsInfo());
  html_source->AddLocalizedString("build_id_name",
                                  IDS_ABOUT_VERSION_BUILD_ID);
  html_source->AddString("build_id", CHROME_BUILD_ID);
#else
  html_source->AddString("os_version", std::string());
  html_source->AddString("flash_plugin", "Flash");
  // Note that the Flash version is retrieve asynchronously and returned in
  // VersionHandler::OnGotPlugins. The area is initially blank.
  html_source->AddString("flash_version", std::string());
#endif  // defined(OS_ANDROID)

  html_source->AddLocalizedString("company", IDS_ABOUT_VERSION_COMPANY_NAME);
  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  html_source->AddString(
      "copyright",
      l10n_util::GetStringFUTF16(IDS_ABOUT_VERSION_COPYRIGHT,
                                 base::IntToString16(exploded_time.year)));
  html_source->AddLocalizedString("revision", IDS_ABOUT_VERSION_REVISION);
  html_source->AddString("cl", version_info.LastChange());
  html_source->AddLocalizedString("official",
      version_info.IsOfficialBuild() ? IDS_ABOUT_VERSION_OFFICIAL :
                                       IDS_ABOUT_VERSION_UNOFFICIAL);
  html_source->AddLocalizedString("user_agent_name",
                                  IDS_ABOUT_VERSION_USER_AGENT);
  html_source->AddString("useragent", GetUserAgent());
  html_source->AddLocalizedString("command_line_name",
                                  IDS_ABOUT_VERSION_COMMAND_LINE);

#if defined(OS_WIN)
  html_source->AddString("command_line", base::WideToUTF16(
      CommandLine::ForCurrentProcess()->GetCommandLineString()));
#elif defined(OS_POSIX)
  std::string command_line;
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  html_source->AddString("command_line", command_line);
#endif

  // Note that the executable path and profile path are retrieved asynchronously
  // and returned in VersionHandler::OnGotFilePaths. The area is initially
  // blank.
  html_source->AddLocalizedString("executable_path_name",
                                  IDS_ABOUT_VERSION_EXECUTABLE_PATH);
  html_source->AddString("executable_path", std::string());

  html_source->AddLocalizedString("profile_path_name",
                                  IDS_ABOUT_VERSION_PROFILE_PATH);
  html_source->AddString("profile_path", std::string());

  html_source->AddLocalizedString("variations_name",
                                  IDS_ABOUT_VERSION_VARIATIONS);

  html_source->SetUseJsonJSFormatV2();
  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath("version.js", IDR_ABOUT_VERSION_JS);
  html_source->AddResourcePath("about_version.css", IDR_ABOUT_VERSION_CSS);
  html_source->SetDefaultResource(IDR_ABOUT_VERSION_HTML);
  return html_source;
}

}  // namespace

VersionUI::VersionUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

#if defined(OS_CHROMEOS)
  web_ui->AddMessageHandler(new VersionHandlerChromeOS());
#else
  web_ui->AddMessageHandler(new VersionHandler());
#endif

#if defined(ENABLE_THEMES)
  // Set up the chrome://theme/ source.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);
#endif

  content::WebUIDataSource::Add(profile, CreateVersionUIDataSource(profile));
}

VersionUI::~VersionUI() {
}
