// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

namespace extension_urls {

std::string GetWebstoreLaunchURL() {
  std::string gallery_prefix = kGalleryBrowsePrefix;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAppsGalleryURL))
    gallery_prefix = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        switches::kAppsGalleryURL);
  if (EndsWith(gallery_prefix, "/", true))
    gallery_prefix = gallery_prefix.substr(0, gallery_prefix.length() - 1);
  return gallery_prefix;
}

std::string GetExtensionGalleryURL() {
  return GetWebstoreLaunchURL() + "/category/extensions";
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL() + "/detail/";
}

GURL GetWebstoreItemJsonDataURL(const std::string& extension_id) {
  return GURL(GetWebstoreLaunchURL() + "/inlineinstall/detail/" + extension_id);
}

GURL GetWebstoreJsonSearchUrl(const std::string& query, const std::string& hl) {
  GURL url(GetWebstoreLaunchURL() + "/jsonsearch");
  url = net::AppendQueryParameter(url, "q", query);
  url = net::AppendQueryParameter(url, "hl", hl);
  return url;
}

GURL GetWebstoreSearchPageUrl(const std::string& query) {
  return GURL(GetWebstoreLaunchURL() + "/search/" +
              net::EscapeQueryParamValue(query, false));
}

const char kGalleryUpdateHttpsUrl[] =
    "https://clients2.google.com/service/update2/crx";
// TODO(battre): Delete the HTTP URL once the blacklist is downloaded via HTTPS.
const char kExtensionBlocklistUrlPrefix[] =
    "http://www.gstatic.com/chrome/extensions/blacklist";
const char kExtensionBlocklistHttpsUrlPrefix[] =
    "https://www.gstatic.com/chrome/extensions/blacklist";

GURL GetWebstoreUpdateUrl() {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kAppsGalleryUpdateURL))
    return GURL(cmdline->GetSwitchValueASCII(switches::kAppsGalleryUpdateURL));
  else
    return GURL(kGalleryUpdateHttpsUrl);
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  if (update_url == store_url) {
    return true;
  } else {
    return (update_url.host() == store_url.host() &&
            update_url.path() == store_url.path());
  }
}

bool IsBlacklistUpdateUrl(const GURL& url) {
  // The extension blacklist URL is returned from the update service and
  // therefore not determined by Chromium. If the location of the blacklist file
  // ever changes, we need to update this function. A DCHECK in the
  // ExtensionUpdater ensures that we notice a change. This is the full URL
  // of a blacklist:
  // http://www.gstatic.com/chrome/extensions/blacklist/l_0_0_0_7.txt
  return StartsWithASCII(url.spec(), kExtensionBlocklistUrlPrefix, true) ||
      StartsWithASCII(url.spec(), kExtensionBlocklistHttpsUrlPrefix, true);
}

const char kGalleryBrowsePrefix[] = "https://chrome.google.com/webstore";

const char kWebstoreSourceField[] = "utm_source";

const char kLaunchSourceAppList[] = "chrome-app-launcher";
const char kLaunchSourceAppListSearch[] = "chrome-app-launcher-search";
const char kLaunchSourceAppListInfoDialog[] = "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

const char kBookmarkManagerId[] = "eemcgdkfndhakfknompkggombfjjjeno";
const char kChromeAppId[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";
const char kCloudPrintAppId[] = "mfehgcgbbipciphmccgaenjidiccnmng";
const char kEasyUnlockAppId[] = "mkaemigholebcgchlkbankmihknojeak";
const char kEnterpriseWebStoreAppId[] = "afchcafgojfnemjkcbhfekplkmjaldaa";
const char kGmailAppId[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kGoogleDocAppId[] = "aohghmighlieiainnegkcijnfilokake";
const char kGooglePlayMusicAppId[] = "icppfcnhkcmnfdhfhphakoifcfokfdhg";
const char kGoogleSearchAppId[] = "coobgpohoikkiipiblmjeljniedjpjpf";
const char kGoogleSheetsAppId[] = "felcaaldnbdncclmgdcncolpebgiejap";
const char kGoogleSlidesAppId[] = "aapocclcgogkmnckokdopfmhonfmgoek";
const char kHTermAppId[] = "pnhechapfaindjhompbnflcldabbghjo";
const char kHTermDevAppId[] = "okddffdblfhhnmhodogpojmfkjmhinfp";
const char kIdentityApiUiAppId[] = "ahjaciijnoiaklcomgnblndopackapon";
const char kCroshBuiltinAppId[] = "nkoccljplnhpfnfiajclkommnmllphnl";
const char kHotwordExtensionId[] = "bepbmhgboaologfdajaanbcjmnhjmhfn";
const char kPdfExtensionId[] = "mhjfbmdgcfjbbpaeojofohoefgiehjai";
const char kQuickOfficeComponentExtensionId[] =
    "bpmcpldpdmajfigpchkicefoigmkfalc";
const char kQuickOfficeInternalExtensionId[] =
    "ehibbfinohgbchlgdbfpikodjaojhccn";
const char kQuickOfficeExtensionId[] = "gbkeegbaiigmenfmjfclcdgdpimamgkj";
const char kSettingsAppId[] = "ennkphjdgehloodpbhlhldgbnhmacadg";
const char kStreamsPrivateTestExtensionId[] =
    "oickdpebdnfbgkcaoklfcdhjniefkcji";
const char kWebStoreAppId[] = "ahfgeienlihckogmohjhadlkjgocpleb";
const char kYoutubeAppId[] = "blpcfgokakmgnkcojhhkbfbldkacnbeo";
const char kInAppPaymentsSupportAppId[] = "nmmhkkegccagdldgiimedpiccmgmieda";

const char kAppLaunchHistogram[] = "Extensions.AppLaunch";
const char kPlatformAppLaunchHistogram[] = "Apps.AppLaunch";
#if defined(OS_CHROMEOS)
// The extension id for the built-in component extension.
const char kChromeVoxExtensionId[] =
    "mndnfokpggljbaajbnioimlmbfngpief";
const char kChromeVoxExtensionPath[] = "chromeos/chromevox";
const char kConnectivityDiagnosticsPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics";
const char kConnectivityDiagnosticsLauncherPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics_launcher";
const char kSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
const char kSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
const char kWallpaperManagerId[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";
const char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
#else
// The extension id for the web store extension.
const char kChromeVoxExtensionId[] =
    "kgejglhpjiefppelpmljglcjbhoiplfn";
#endif

const char kAppStateNotInstalled[] = "not_installed";
const char kAppStateInstalled[] = "installed";
const char kAppStateDisabled[] = "disabled";
const char kAppStateRunning[] = "running";
const char kAppStateCannotRun[] = "cannot_run";
const char kAppStateReadyToRun[] = "ready_to_run";

const char kMediaFileSystemPathPart[] = "_";

}  // namespace extension_misc
