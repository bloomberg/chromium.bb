// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Apps/extensions explicitly whitelisted for use in public sessions.
const char* kPublicSessionWhitelist[] = {
    // Public sessions in general:
    "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
    "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer

    // Libraries:
    "aclofikceldphonlfmghmimkodjdmhck",  // Ancoris login component
    "eilbnahdgoddoedakcmfkcgfoegeloil",  // Ancoris proxy component
    "ceehlgckkmkaoggdnjhibffkphfnphmg",  // Libdata login

    // Retail mode:
    "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds demo
    "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled demo
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar demo
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "cdjikkcakjcdjemakobkmijmikhkegcj",  // Chrome Remote Desktop demo
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
    "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer demo
    "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app demo
    "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote demo
    "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail demo
    "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive demo
    "dondgdlndnpianbklfnehgdhkickdjck",  // Google Keep demo
    "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+ demo
    "ifpkhncdnjfipfjlhfidljjffdgklanh",  // Google+ Photos demo
    "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app demo
    "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension demo
    "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle demo
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app demo
    "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app demo
    "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix demo
    "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times demo
    "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora demo
    "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr demo
    "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice demo
    "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets demo
    "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides demo
    "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify demo
    "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app demo
    "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck demo
    "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube demo

    // Testing extensions:
    "ongnjlefhnoajpbodoldndkbkdgfomlp",  // Show Managed Storage
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
    base::string16* error) const {
  if (account_type_ == policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION) {
    // Allow extension if it is an externally hosted component of Chrome.
    if (extension->location() ==
        extensions::Manifest::EXTERNAL_COMPONENT) {
      return true;
    }

    // Allow extension if its type is whitelisted for use in public sessions.
    if (extension->GetType() == extensions::Manifest::TYPE_HOSTED_APP)
      return true;

    // Allow extension if its specific ID is whitelisted for use in public
    // sessions.
    for (size_t i = 0; i < arraysize(kPublicSessionWhitelist); ++i) {
      if (extension->id() == kPublicSessionWhitelist[i])
        return true;
    }
  } else if (account_type_ == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    // For single-app kiosk sessions, allow only platform apps.
    if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP)
      return true;
  }

  // Disallow all other extensions.
  if (error) {
    *error = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_CANT_INSTALL_IN_DEVICE_LOCAL_ACCOUNT,
          base::UTF8ToUTF16(extension->name()),
          base::UTF8ToUTF16(extension->id()));
  }
  return false;
}

}  // namespace chromeos
