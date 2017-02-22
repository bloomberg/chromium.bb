// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/common/extensions/extension_constants.h"

namespace extension_urls {

namespace {

const char kGalleryUpdateHttpsUrl[] =
    "https://clients2.google.com/service/update2/crx";

}  // namespace

const char* GetDefaultWebstoreUpdateUrl() {
  return kGalleryUpdateHttpsUrl;
}

const char kWebstoreSourceField[] = "utm_source";

const char kLaunchSourceAppList[] = "chrome-app-launcher";
const char kLaunchSourceAppListSearch[] = "chrome-app-launcher-search";
const char kLaunchSourceAppListInfoDialog[] = "chrome-app-launcher-info-dialog";

}  // namespace extension_urls

namespace extension_misc {

const char kBookmarkManagerId[] = "eemcgdkfndhakfknompkggombfjjjeno";
const char kChromeAppId[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";
const char kCloudPrintAppId[] = "mfehgcgbbipciphmccgaenjidiccnmng";
const char kDataSaverExtensionId[] = "pfmgfdlgomnbgkofeojodiodmgpgmkac";
const char kDriveExtensionId[] = "ghbmnnjooekpmoecnnnilnnbdlolhkhi";
const char kDriveHostedAppId[] = "apdfllckaahabafndbhieahigkjlhalf";
const char kEasyUnlockAppId[] = "mkaemigholebcgchlkbankmihknojeak";
const char kEnterpriseWebStoreAppId[] = "afchcafgojfnemjkcbhfekplkmjaldaa";
const char kFeedbackExtensionId[] = "gfdkimpbcpahaombhbimeihdjnejgicl";
const char kGmailAppId[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kGoogleDocAppId[] = "aohghmighlieiainnegkcijnfilokake";
const char kGooglePlayMusicAppId[] = "icppfcnhkcmnfdhfhphakoifcfokfdhg";
const char kGoogleSheetsAppId[] = "felcaaldnbdncclmgdcncolpebgiejap";
const char kGoogleSlidesAppId[] = "aapocclcgogkmnckokdopfmhonfmgoek";
const char kHTermAppId[] = "pnhechapfaindjhompbnflcldabbghjo";
const char kHTermDevAppId[] = "okddffdblfhhnmhodogpojmfkjmhinfp";
const char kIdentityApiUiAppId[] = "ahjaciijnoiaklcomgnblndopackapon";
const char kCroshBuiltinAppId[] = "nkoccljplnhpfnfiajclkommnmllphnl";
const char kHotwordAudioVerificationAppId[] =
    "abjokfonkihficiokmkfboogholifghn";
const char kHotwordNewExtensionId[] = "nbpagnldghgfoolbancepceaanlmhfmd";
const char kHotwordSharedModuleId[] = "lccekmodgklaepjeofjdjpbminllajkg";
const char kYoutubeAppId[] = "blpcfgokakmgnkcojhhkbfbldkacnbeo";
const char kInAppPaymentsSupportAppId[] = "nmmhkkegccagdldgiimedpiccmgmieda";
#if defined(ENABLE_MEDIA_ROUTER)
const char kMediaRouterStableExtensionId[] = "pkedcjkdefgpdelpbcmbmeomcjbeemfm";
#endif  // defined(ENABLE_MEDIA_ROUTER)

#if defined(OS_CHROMEOS)
// The extension id for the built-in component extension.
const char kChromeVoxExtensionId[] = "mndnfokpggljbaajbnioimlmbfngpief";
const char kChromeVoxExtensionPath[] = "chromeos/chromevox";
const char kSelectToSpeakExtensionId[] = "klbcgckkldhdhonijdbnhhaiedfkllef";
const char kSelectToSpeakExtensionPath[] = "chromeos/select_to_speak";
const char kSwitchAccessExtensionId[] = "pmehocpgjmkenlokgjfkaichfjdhpeol";
const char kSwitchAccessExtensionPath[] = "chromeos/switch_access";
const char kGuestManifestFilename[] = "manifest_guest.json";
const char kBrailleImeExtensionId[] =
    "jddehjeebkoimngcbdkaahpobgicbffp";
const char kBrailleImeExtensionPath[] =
    "chromeos/braille_ime";
const char kBrailleImeEngineId[] =
    "_comp_ime_jddehjeebkoimngcbdkaahpobgicbffpbraille";
const char kConnectivityDiagnosticsPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics";
const char kConnectivityDiagnosticsLauncherPath[] =
    "/usr/share/chromeos-assets/connectivity_diagnostics_launcher";
const char kFirstRunDialogId[] = "jdgcneonijmofocbhmijhacgchbihela";
const char kSpeechSynthesisExtensionPath[] =
    "/usr/share/chromeos-assets/speech_synthesis/patts";
const char kSpeechSynthesisExtensionId[] =
    "gjjabgpgjpampikjhjpfhneeoapjbjaf";
const char kWallpaperManagerId[] = "obklkkbkpaoaejdabbfldmcfplpdgolj";
const char kWebstoreWidgetAppId[] = "fbjakikfhfdajcamjleinfciajelkpek";
const char kZIPUnpackerExtensionId[] = "oedeeodfidgoollimchfdnbmhcpnklnd";
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
