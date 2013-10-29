// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "extensions/common/manifest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Apps/extensions explicitly whitelisted for use in device-local accounts.
const char* kDeviceLocalAccountWhitelist[] = {
  // Public sessions in general:
  "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
  "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
  "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer

  // Retail mode:
  "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds
  "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled
  "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
  "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar
  "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
  "cdjikkcakjcdjemakobkmijmikhkegcj",  // Chrome Remote Desktop
  "jkoildpomkimndcphjpffmephmcmkfhn",  // Chromebook Demo App
  "ielkookhdphmgbipcfmafkaiagademfp",  // Custom bookmarks
  "kogjlbfgggambihdjcpijgcbmenblimd",  // Custom bookmarks
  "ogbkmlkceflgpilgbmbcfbifckpkfacf",  // Custom bookmarks
  "pbbbjjecobhljkkcenlakfnkmkfkfamd",  // Custom bookmarks
  "jkbfjmnjcdmhlfpephomoiipbhcoiffb",  // Custom bookmarks
  "dgmblbpgafgcgpkoiilhjifindhinmai",  // Custom bookmarks
  "iggnealjakkgfofealilhkkclnbnfnmo",  // Custom bookmarks
  "lplkobnahgbopmpkdapaihnnojkphahc",  // Custom bookmarks
  "lejnflfhjpcannpaghnahbedlabpmhoh",  // Custom bookmarks
  "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer
  "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app
  "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote
  "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail
  "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive
  "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+
  "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app
  "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension
  "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle
  "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app
  "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app
  "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix
  "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times
  "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora
  "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr
  "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice
  "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets
  "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides
  "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify
  "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app
  "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck
  "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube
};

}  // namespace

DeviceLocalAccountManagementPolicyProvider::
    DeviceLocalAccountManagementPolicyProvider(
        policy::DeviceLocalAccount::Type account_type)
    : account_type_(account_type) {
}

DeviceLocalAccountManagementPolicyProvider::
    ~DeviceLocalAccountManagementPolicyProvider() {
}

std::string DeviceLocalAccountManagementPolicyProvider::
    GetDebugPolicyProviderName() const {
#if defined(NDEBUG)
  NOTREACHED();
  return std::string();
#else
  return "whitelist for device-local accounts";
#endif
}

bool DeviceLocalAccountManagementPolicyProvider::UserMayLoad(
    const extensions::Extension* extension,
    string16* error) const {
  if (account_type_ == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    // For single-app kiosk sessions, allow only platform apps.
    if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP)
      return true;

  } else {
    // Allow extension if its type is whitelisted for use in device-local
    // accounts.
    if (extension->GetType() == extensions::Manifest::TYPE_HOSTED_APP)
      return true;

    // Allow extension if its specific ID is whitelisted for use in device-local
    // accounts.
    for (size_t i = 0; i < arraysize(kDeviceLocalAccountWhitelist); ++i) {
      if (extension->id() == kDeviceLocalAccountWhitelist[i])
        return true;
    }
  }

  // Disallow all other extensions.
  if (error) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_IN_DEVICE_LOCAL_ACCOUNT,
          UTF8ToUTF16(extension->name()),
          UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace chromeos
