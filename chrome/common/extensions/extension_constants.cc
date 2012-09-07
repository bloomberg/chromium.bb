// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

#include <vector>

#include "base/command_line.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_util.h"

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

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL() + "/detail/";
}

GURL GetWebstoreIntentQueryURL(const std::string& action,
                               const std::string& type) {
  const char kIntentsCategoryPath[] = "category/collection/webintent_apps";

  GURL url(std::string(kGalleryBrowsePrefix) + "/");
  url = url.Resolve(kIntentsCategoryPath);
  url = chrome_common_net::AppendQueryParameter(url, "_wi", action);
  url = chrome_common_net::AppendQueryParameter(url, "_mt", type);

  return url;
}

GURL GetWebstoreItemJsonDataURL(const std::string& extension_id) {
  return GURL(GetWebstoreLaunchURL() + "/inlineinstall/detail/" + extension_id);
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

}  // namespace extension_urls

namespace extension_filenames {

const char kTempExtensionName[] = "CRX_INSTALL";

// The file to write our decoded images to, relative to the extension_path.
const char kDecodedImagesFilename[] = "DECODED_IMAGES";

// The file to write our decoded message catalogs to, relative to the
// extension_path.
const char kDecodedMessageCatalogsFilename[] = "DECODED_MESSAGE_CATALOGS";

const char kGeneratedBackgroundPageFilename[] =
    "_generated_background_page.html";
}

// These must match the values expected by the chrome.management extension API.
namespace extension_info_keys {
  const char kDescriptionKey[] = "description";
  const char kEnabledKey[] = "enabled";
  const char kHomepageUrlKey[] = "homepageUrl";
  const char kIdKey[] = "id";
  const char kNameKey[] = "name";
  const char kOfflineEnabledKey[] = "offlineEnabled";
  const char kOptionsUrlKey[] = "optionsUrl";
  const char kDetailsUrlKey[] = "detailsUrl";
  const char kVersionKey[] = "version";

}  // namespace extension_filenames

namespace extension_misc {
const char kBookmarkManagerId[] = "eemcgdkfndhakfknompkggombfjjjeno";
const char kCitrixReceiverAppId[] = "haiffjcadagjlijoggckpgfnoeiflnem";
const char kCitrixReceiverAppBetaId[] = "gnedhmakppccajfpfiihfcdlnpgomkcf";
const char kCitrixReceiverAppDevId[] = "fjcibdnjlbfnbfdjneajpipnlcppleek";
const char kEnterpriseWebStoreAppId[] = "afchcafgojfnemjkcbhfekplkmjaldaa";
const char kHTermAppId[] = "pnhechapfaindjhompbnflcldabbghjo";
const char kHTermDevAppId[] = "okddffdblfhhnmhodogpojmfkjmhinfp";
const char kCroshBuiltinAppId[] = "nkoccljplnhpfnfiajclkommnmllphnl";
const char kWebStoreAppId[] = "ahfgeienlihckogmohjhadlkjgocpleb";
const char kCloudPrintAppId[] = "mfehgcgbbipciphmccgaenjidiccnmng";
const char kChromeAppId[] = "mgndgikekgjfcpckkfioiadnlibdjbkf";
const char kTalkExtensionId[] = "nckgahadagoaajjgafhacjanaoiihapd";
const char kTalkBetaExtensionId[] = "eggnbpckecmjlblplehfpjjdhhidfdoj";
const char kTalkAlphaExtensionId[] = "ppleadejekpmccmnpjdimmlfljlkdfej";
const char kTalkDebugExtensionId[] = "ljclpkphhpbpinifbeabbhlfddcpfdde";
const char kAppLaunchHistogram[] = "Extensions.AppLaunch";
#if defined(OS_CHROMEOS)
const char kAccessExtensionPath[] =
    "/usr/share/chromeos-assets/accessibility/extensions";
const char kChromeVoxDirectoryName[] = "access_chromevox";
#endif

const char kAppStateNotInstalled[] = "not_installed";
const char kAppStateInstalled[] = "installed";
const char kAppStateDisabled[] = "disabled";
const char kAppStateRunning[] = "running";
const char kAppStateCannotRun[] = "cannot_run";
const char kAppStateReadyToRun[] = "ready_to_run";

const char kMediaFileSystemPathPart[] = "_";

const char kAppNotificationsIncognitoError[] =
    "This API is not accessible by 'split' mode "
    "extensions in incognito windows.";

const int kExtensionIconSizes[] = {
  EXTENSION_ICON_GIGANTOR,  // 512
  EXTENSION_ICON_EXTRA_LARGE,  // 256
  EXTENSION_ICON_LARGE,  // 128
  EXTENSION_ICON_MEDIUM,  // 48
  EXTENSION_ICON_SMALL,  // 32
  EXTENSION_ICON_SMALLISH,  // 24
  EXTENSION_ICON_BITTY  // 16
};

const size_t kNumExtensionIconSizes =
    arraysize(kExtensionIconSizes);

}  // namespace extension_misc
