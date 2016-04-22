// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/version_ui.h"

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version_handler.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_ui_constants.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/user_agent.h"
#include "grit/browser_resources.h"
#include "grit/components_chromium_strings.h"
#include "grit/components_google_chrome_strings.h"
#include "grit/components_resources.h"
#include "grit/components_strings.h"
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

using content::WebUIDataSource;

namespace {

WebUIDataSource* CreateVersionUIDataSource() {
  WebUIDataSource* html_source =
      WebUIDataSource::Create(chrome::kChromeUIVersionHost);

  // Localized and data strings.
  html_source->AddLocalizedString(version_ui::kTitle, IDS_VERSION_UI_TITLE);
  html_source->AddLocalizedString(version_ui::kApplicationLabel,
                                  IDS_PRODUCT_NAME);
  html_source->AddString(version_ui::kVersion,
                         version_info::GetVersionNumber());
  html_source->AddString(version_ui::kVersionModifier,
                         chrome::GetChannelString());
  html_source->AddLocalizedString(version_ui::kOSName, IDS_VERSION_UI_OS);
  html_source->AddLocalizedString(version_ui::kARC, IDS_ARC_LABEL);
  html_source->AddLocalizedString(version_ui::kPlatform, IDS_PLATFORM_LABEL);
  html_source->AddString(version_ui::kOSType, version_info::GetOSType());
  html_source->AddString(version_ui::kBlinkVersion,
                         content::GetWebKitVersion());
  html_source->AddString(version_ui::kJSEngine, "V8");
  html_source->AddString(version_ui::kJSVersion, v8::V8::GetVersion());

#if defined(OS_ANDROID)
  html_source->AddString(version_ui::kOSVersion,
                         AndroidAboutAppInfo::GetOsInfo());
  html_source->AddLocalizedString(version_ui::kBuildIDName,
                                  IDS_VERSION_UI_BUILD_ID);
  html_source->AddString(version_ui::kBuildID, CHROME_BUILD_ID);
#else
  html_source->AddString(version_ui::kOSVersion, std::string());
  html_source->AddString(version_ui::kFlashPlugin, "Flash");
  // Note that the Flash version is retrieve asynchronously and returned in
  // VersionHandler::OnGotPlugins. The area is initially blank.
  html_source->AddString(version_ui::kFlashVersion, std::string());
#endif  // defined(OS_ANDROID)

  html_source->AddLocalizedString(version_ui::kCompany,
                                  IDS_ABOUT_VERSION_COMPANY_NAME);
  base::Time::Exploded exploded_time;
  base::Time::Now().LocalExplode(&exploded_time);
  html_source->AddString(
      version_ui::kCopyright,
      l10n_util::GetStringFUTF16(IDS_ABOUT_VERSION_COPYRIGHT,
                                 base::IntToString16(exploded_time.year)));
  html_source->AddLocalizedString(version_ui::kRevision,
                                  IDS_VERSION_UI_REVISION);
  html_source->AddString(version_ui::kCL, version_info::GetLastChange());
  html_source->AddLocalizedString(version_ui::kOfficial,
                                  version_info::IsOfficialBuild()
                                      ? IDS_VERSION_UI_OFFICIAL
                                      : IDS_VERSION_UI_UNOFFICIAL);
#if defined(ARCH_CPU_64_BITS)
  html_source->AddLocalizedString(version_ui::kVersionBitSize,
                                  IDS_VERSION_UI_64BIT);
#else
  html_source->AddLocalizedString(version_ui::kVersionBitSize,
                                  IDS_VERSION_UI_32BIT);
#endif
  html_source->AddLocalizedString(version_ui::kUserAgentName,
                                  IDS_VERSION_UI_USER_AGENT);
  html_source->AddString(version_ui::kUserAgent, GetUserAgent());
  html_source->AddLocalizedString(version_ui::kCommandLineName,
                                  IDS_VERSION_UI_COMMAND_LINE);

#if defined(OS_WIN)
  html_source->AddString(
      version_ui::kCommandLine,
      base::CommandLine::ForCurrentProcess()->GetCommandLineString());
#elif defined(OS_POSIX)
  std::string command_line;
  typedef std::vector<std::string> ArgvList;
  const ArgvList& argv = base::CommandLine::ForCurrentProcess()->argv();
  for (ArgvList::const_iterator iter = argv.begin(); iter != argv.end(); iter++)
    command_line += " " + *iter;
  // TODO(viettrungluu): |command_line| could really have any encoding, whereas
  // below we assumes it's UTF-8.
  html_source->AddString(version_ui::kCommandLine, command_line);
#endif

  // Note that the executable path and profile path are retrieved asynchronously
  // and returned in VersionHandler::OnGotFilePaths. The area is initially
  // blank.
  html_source->AddLocalizedString(version_ui::kExecutablePathName,
                                  IDS_VERSION_UI_EXECUTABLE_PATH);
  html_source->AddString(version_ui::kExecutablePath, std::string());

  html_source->AddLocalizedString(version_ui::kProfilePathName,
                                  IDS_VERSION_UI_PROFILE_PATH);
  html_source->AddString(version_ui::kProfilePath, std::string());

  html_source->AddLocalizedString(version_ui::kVariationsName,
                                  IDS_VERSION_UI_VARIATIONS);

#if defined(OS_WIN)
#if defined(__clang__)
  html_source->AddString("compiler", "clang");
#elif defined(_MSC_VER) && _MSC_VER == 1900
  html_source->AddString("compiler", "MSVC 2015");
#elif defined(_MSC_VER) && _MSC_VER == 1800
  html_source->AddString("compiler", "MSVC 2013");
#elif defined(_MSC_VER)
#error "Unsupported version of MSVC."
#else
  html_source->AddString("compiler", "Unknown");
#endif
#endif  // defined(OS_WIN)

  html_source->SetJsonPath("strings.js");
  html_source->AddResourcePath(version_ui::kVersionJS, IDR_VERSION_UI_JS);
  html_source->AddResourcePath(version_ui::kAboutVersionCSS,
                               IDR_VERSION_UI_CSS);
  html_source->SetDefaultResource(IDR_VERSION_UI_HTML);
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

  WebUIDataSource::Add(profile, CreateVersionUIDataSource());
}

VersionUI::~VersionUI() {
}
