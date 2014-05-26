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
    return GetDefaultWebstoreUpdateUrl();
}

GURL GetDefaultWebstoreUpdateUrl() {
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
const char kChromeVoxManifestFilename[] = "manifest.json";
const char kChromeVoxGuestManifestFilename[] = "manifest_guest.json";
const char kBrailleImeExtensionId[] =
    "jddehjeebkoimngcbdkaahpobgicbffp";
const char kBrailleImeExtensionPath[] =
    "chromeos/braille_ime";
const char kBrailleImeEngineId[] =
    "_comp_ime_jddehjeebkoimngcbdkaahpobgicbffpbraille";
const char kConnectivityDiagnosticsPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics";
const char kConnectivityDiagnosticsKioskPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics_kiosk";
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

const uint8 kWebstoreSignaturesPublicKey[] = {
    0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
    0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0x8f, 0xfb, 0xbf,
    0x5c, 0x37, 0x63, 0x94, 0x3c, 0xb0, 0xee, 0x01, 0xc4, 0xb5, 0xa6, 0x9a,
    0xb1, 0x9f, 0x46, 0x74, 0x6f, 0x16, 0x38, 0xa0, 0x32, 0x27, 0x35, 0xdd,
    0xf0, 0x71, 0x6b, 0x0e, 0xdc, 0xf6, 0x25, 0xcb, 0xb2, 0xed, 0xea, 0xfb,
    0x32, 0xd5, 0xaf, 0x1e, 0x03, 0x43, 0x03, 0x46, 0xf0, 0xa7, 0x39, 0xdb,
    0x23, 0x96, 0x1d, 0x65, 0xe5, 0x78, 0x51, 0xf0, 0x84, 0xb0, 0x0e, 0x12,
    0xac, 0x0e, 0x5b, 0xdc, 0xc9, 0xd6, 0x4c, 0x7c, 0x00, 0xd5, 0xb8, 0x1b,
    0x88, 0x33, 0x3e, 0x2f, 0xda, 0xeb, 0xaa, 0xf7, 0x1a, 0x75, 0xc2, 0xae,
    0x3a, 0x54, 0xde, 0x37, 0x8f, 0x10, 0xd2, 0x28, 0xe6, 0x84, 0x79, 0x4d,
    0x15, 0xb4, 0xf3, 0xbd, 0x3f, 0x56, 0xd3, 0x3c, 0x3f, 0x18, 0xab, 0xfc,
    0x2e, 0x05, 0xc0, 0x1e, 0x08, 0x31, 0xb6, 0x61, 0xd0, 0xfd, 0x9f, 0x4f,
    0x3f, 0x64, 0x0d, 0x17, 0x93, 0xbc, 0xad, 0x41, 0xc7, 0x48, 0xbe, 0x00,
    0x27, 0xa8, 0x4d, 0x70, 0x42, 0x92, 0x05, 0x54, 0xa6, 0x6d, 0xb8, 0xde,
    0x56, 0x6e, 0x20, 0x49, 0x70, 0xee, 0x10, 0x3e, 0x6b, 0xd2, 0x7c, 0x31,
    0xbd, 0x1b, 0x6e, 0xa4, 0x3c, 0x46, 0x62, 0x9f, 0x08, 0x66, 0x93, 0xf9,
    0x2a, 0x51, 0x31, 0xa8, 0xdb, 0xb5, 0x9d, 0xb9, 0x0f, 0x73, 0xe8, 0xa0,
    0x09, 0x32, 0x01, 0xe9, 0x7b, 0x2a, 0x8a, 0x36, 0xa0, 0xcf, 0x17, 0xb0,
    0x50, 0x70, 0x9d, 0xa2, 0xf9, 0xa4, 0x6f, 0x62, 0x4d, 0xb6, 0xc9, 0x31,
    0xfc, 0xf3, 0x08, 0x12, 0xff, 0x93, 0xbd, 0x62, 0x31, 0xd8, 0x1c, 0xea,
    0x1a, 0x9e, 0xf5, 0x81, 0x28, 0x7f, 0x75, 0x5e, 0xd2, 0x27, 0x7a, 0xc2,
    0x96, 0xf5, 0x9d, 0xdb, 0x18, 0xfc, 0x76, 0xdc, 0x46, 0xf0, 0x57, 0xc0,
    0x58, 0x34, 0xc8, 0x22, 0x2d, 0x2a, 0x65, 0x75, 0xa7, 0xd9, 0x08, 0x62,
    0xcd, 0x02, 0x03, 0x01, 0x00, 0x01};

const int kWebstoreSignaturesPublicKeySize =
    arraysize(kWebstoreSignaturesPublicKey);

}  // namespace extension_misc
