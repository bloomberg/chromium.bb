// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/device_local_account_management_policy_provider.h"

#include <stddef.h>

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

namespace emk = extensions::manifest_keys;

// Apps/extensions explicitly whitelisted for use in public sessions.
const char* const kPublicSessionWhitelist[] = {
    // Public sessions in general:
    "cbkkbcmdlboombapidmoeolnmdacpkch",  // Chrome RDP
    "djflhoibgkdhkhhcedjiklpkjnoahfmg",  // User Agent Switcher
    "iabmpiboiopbgfabjmgeedhcmjenhbla",  // VNC Viewer
    "haiffjcadagjlijoggckpgfnoeiflnem",  // Citrix Receiver
    "lfnfbcjdepjffcaiagkdmlmiipelnfbb",  // Citrix Receiver (branded)
    "mfaihdlpglflfgpfjcifdjdjcckigekc",  // ARC Runtime
    "ngjnkanfphagcaokhjecbgkboelgfcnf",  // Print button
    "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
    "cjanmonomjogheabiocdamfpknlpdehm",  // HP printer driver
    "pmnllmkmjilbojkpgplbdmckghmaocjh",  // Scan app by François Beaufort
    "khpfeaanjngmcnplbdlpegiifgpfgdco",  // SmartCard Manager App
    "dojmlffgommefdofbfdajjpgjgjoffjo",  // Charismathics Smart Card Middleware

    // Libraries:
    "aclofikceldphonlfmghmimkodjdmhck",  // Ancoris login component
    "eilbnahdgoddoedakcmfkcgfoegeloil",  // Ancoris proxy component
    "ceehlgckkmkaoggdnjhibffkphfnphmg",  // Libdata login
    "fnhgfoccpcjdnjcobejogdnlnidceemb",  // OverDrive

    // Education:
    "cmeclblmdmffdgpdlifgepjddoplmmal",  //  Imagine Learning

    // Retail mode:
    "bjfeaefhaooblkndnoabbkkkenknkemb",  // 500 px demo
    "ehcabepphndocfmgbdkbjibfodelmpbb",  // Angry Birds demo
    "kgimkbnclbekdkabkpjhpakhhalfanda",  // Bejeweled demo
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "fpgfohogebplgnamlafljlcidjedbdeb",  // Calendar demo
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "cdjikkcakjcdjemakobkmijmikhkegcj",  // Chrome Remote Desktop demo
    "jkoildpomkimndcphjpffmephmcmkfhn",  // Chromebook Demo App
    "lbhdhapagjhalobandnbdnmblnmocojh",  // Crackle demo
    "ielkookhdphmgbipcfmafkaiagademfp",  // Custom bookmarks
    "kogjlbfgggambihdjcpijgcbmenblimd",  // Custom bookmarks
    "ogbkmlkceflgpilgbmbcfbifckpkfacf",  // Custom bookmarks
    "pbbbjjecobhljkkcenlakfnkmkfkfamd",  // Custom bookmarks
    "jkbfjmnjcdmhlfpephomoiipbhcoiffb",  // Custom bookmarks
    "dgmblbpgafgcgpkoiilhjifindhinmai",  // Custom bookmarks
    "iggnealjakkgfofealilhkkclnbnfnmo",  // Custom bookmarks
    "lplkobnahgbopmpkdapaihnnojkphahc",  // Custom bookmarks
    "lejnflfhjpcannpaghnahbedlabpmhoh",  // Custom bookmarks
    "dhjmfhojkfjmfbnbnpichdmcdghdpccg",  // Cut the Rope demo
    "ebkhfdfghngbimnpgelagnfacdafhaba",  // Deezer demo
    "npnjdccdffhdndcbeappiamcehbhjibf",  // Docs.app demo
    "ekgadegabdkcbkodfbgidncffijbghhl",  // Duolingo demo
    "iddohohhpmajlkbejjjcfednjnhlnenk",  // Evernote demo
    "bjdhhokmhgelphffoafoejjmlfblpdha",  // Gmail demo
    "nldmakcnfaflagmohifhcihkfgcbmhph",  // Gmail offline demo
    "mdhnphfgagkpdhndljccoackjjhghlif",  // Google Drive demo
    "dondgdlndnpianbklfnehgdhkickdjck",  // Google Keep demo
    "amfoiggnkefambnaaphodjdmdooiinna",  // Google Play Movie and TV demo
    "fgjnkhlabjcaajddbaenilcmpcidahll",  // Google+ demo
    "ifpkhncdnjfipfjlhfidljjffdgklanh",  // Google+ Photos demo
    "cgmlfbhkckbedohgdepgbkflommbfkep",  // Hangouts.app demo
    "ndlgnmfmgpdecjgehbcejboifbbmlkhp",  // Hash demo
    "edhhaiphkklkcfcbnlbpbiepchnkgkpn",  // Helper.extension demo
    "jckncghadoodfbbbmbpldacojkooophh",  // Journal demo
    "diehajhcjifpahdplfdkhiboknagmfii",  // Kindle demo
    "idneggepppginmaklfbaniklagjghpio",  // Kingsroad demo
    "nhpmmldpbfjofkipjaieeomhnmcgihfm",  // Menu.app demo
    "kcjbmmhccecjokfmckhddpmghepcnidb",  // Mint demo
    "onbhgdmifjebcabplolilidlpgeknifi",  // Music.app demo
    "kkkbcoabfhgekpnddfkaphobhinociem",  // Netflix demo
    "adlphlfdhhjenpgimjochcpelbijkich",  // New York Times demo
    "cgefhjmlaifaamhhoojmpcnihlbddeki",  // Pandora demo
    "kpjjigggmcjinapdeipapdcnmnjealll",  // Pixlr demo
    "ifnadhpngkodeccijnalokiabanejfgm",  // Pixsta demo
    "klcojgagjmpgmffcildkgbfmfffncpcd",  // Plex demo
    "nnikmgjhdlphciaonjmoppfckbpoinnb",  // Pocket demo
    "khldngaiohpnnoikfmnmfnebecgeobep",  // Polarr Photo demo
    "aleodiobpjillgfjdkblghiiaegggmcm",  // Quickoffice demo
    "nifkmgcdokhkjghdlgflonppnefddien",  // Sheets demo
    "hdmobeajeoanbanmdlabnbnlopepchip",  // Slides demo
    "ikmidginfdcbojdbmejkeakncgdbmonc",  // Soundtrap demo
    "dgohlccohkojjgkkfholmobjjoledflp",  // Spotify demo
    "dhmdaeekeihmajjnmichlhiffffdbpde",  // Store.app demo
    "onklhlmbpfnmgmelakhgehkfdmkpmekd",  // Todoist demo
    "jeabmjjifhfcejonjjhccaeigpnnjaak",  // TweetDeck demo
    "gnckahkflocidcgjbeheneogeflpjien",  // Vine demo
    "pdckcbpciaaicoomipamcabpdadhofgh",  // Weatherbug demo
    "biliocemfcghhioihldfdmkkhnofcgmb",  // Webcam Toy demo
    "bhfoghflalnnjfcfkaelngenjgjjhapk",  // Wevideo demo
    "pjckdjlmdcofkkkocnmhcbehkiapalho",  // Wunderlist demo
    "pbdihpaifchmclcmkfdgffnnpfbobefh",  // YouTube demo

    // Testing extensions:
    "ongnjlefhnoajpbodoldndkbkdgfomlp",  // Show Managed Storage
    "ilnpadgckeacioehlommkaafedibdeob",  // Enterprise DeviceAttributes
    "oflckobdemeldmjddmlbaiaookhhcngo",  // Citrix Receiver QA version
    "ljacajndfccfgnfohlgkdphmbnpkjflk",  // Chrome Remote Desktop (Dev Build)
};

// List of manifest entries from https://developer.chrome.com/apps/manifest.
// Unsafe entries are commented out and special cases too.
const char* const kSafeManifestEntries[] = {
    // Special-cased in IsPlatformAppSafeForPublicSession().
    // emk::kApp,

    // Special-cased in IsPlatformAppSafeForPublicSession().
    // emk::kManifestVersion,

    // Just a display string.
    emk::kName,

    // Just a display string.
    emk::kShortName,

    // Version string (for app updates).
    emk::kVersion,

    // Name of directory containg default strings.
    emk::kDefaultLocale,

    // An implementation detail (actually written by Chrome, not the app
    // author).
    emk::kCurrentLocale,

    // Just a display string.
    emk::kDescription,

    // Just UX.
    emk::kIcons,

    // Documented in https://developer.chrome.com/extensions/manifest but not
    // implemented anywhere.  Still, a lot of apps use it.
    "author",

    // TBD
    // emk::kBluetooth,

    // TBD
    // emk::kCommands,

    // TBD, doc missing
    // emk::kCopresence,

    // TBD, looks unsafe
    // emk::kEventRules,

    // TBD
    // emk::kExternallyConnectable,

    // TBD
    // emk::kFileHandlers,

    // TBD
    // emk::kFileSystemProviderCapabilities,

    // Shared Modules configuration: Import resources from another extension.
    emk::kImport,

    // Shared Modules configuration: Allow other extensions to access resources.
    emk::kExport,

    // Shared Modules configuration: Specify extension id for development.
    emk::kKey,

    // Descriptive statement about the app.
    emk::kKioskEnabled,

    // Contradicts the purpose of running inside a Public Session.
    // emk::kKioskOnly,

    // Descriptive statement about the app.
    emk::kMinimumChromeVersion,

    // NaCl modules are bound to app permissions just like the rest of the app
    // and thus should not pose a risk.
    emk::kNaClModules,

    // TBD, doc missing
    // emk::kOAuth2,

    // Descriptive statement about the app.
    emk::kOfflineEnabled,

    // Special-cased in IsPlatformAppSafeForPublicSession().
    // emk::kOptionalPermissions,

    // Special-cased in IsPlatformAppSafeForPublicSession().
    // emk::kPermissions,

    // No constant in manifest_constants.cc.
    // "platforms",

    // Descriptive statement about the app.
    emk::kRequirements,

    // Execute some pages in a separate sandbox.  (Note: Using string literal
    // since extensions::manifest_keys only has constants for sub-keys.)
    "sandbox",

    // TBD, doc missing
    // emk::kSignature,

    // Network access.
    emk::kSockets,

    // TBD.  (Note: Using string literal since extensions::manifest_keys only
    // has constants for sub-keys.)
    // "storage",

    // TBD, doc missing
    // emk::kSystemIndicator,

    // TODO(tnagel): Ensure that extension updates query UserMayLoad().
    // https://crbug.com/549720
    emk::kUpdateURL,

    // Apps may intercept navigations to URL patterns for domains for which the
    // app author has proven ownership of to the Web Store.  (Chrome starts the
    // app instead of fulfilling the navigation.)  This is only safe for apps
    // that have been loaded from the Web Store and thus is special-cased in
    // IsPlatformAppSafeForPublicSession().
    // emk::kUrlHandlers,

    // TBD
    // emk::kUsbPrinters,

    // Just a display string.
    emk::kVersionName,

    // Webview has no special privileges or capabilities.
    emk::kWebview,
};

// List of permission strings based on [1] and [2].  See |kSafePermissionDicts|
// for permission dicts.  Since Public Session users may be fully unaware of any
// apps being installed, their consent to access any kind of sensitive
// information cannot be assumed.  Therefore only APIs are whitelisted which
// should not leak sensitive data to the caller.  Since the privacy boundary is
// drawn at the API level, no safeguards are required to prevent exfiltration
// and thus apps may communicate freely over any kind of network.
// [1] https://developer.chrome.com/apps/declare_permissions
// [2] https://developer.chrome.com/apps/api_other
const char* const kSafePermissionStrings[] = {
    // Risky: Reading accessibility settings could allow to infer health
    // information.
    // "accessibilityFeatures.read",

    // Modifying accessibility settings seems safe (at most a user could be
    // confused by it).
    "accessibilityFeatures.modify",

    // Schedule code to run at future times.
    "alarms",

    // Risk of listening attack.
    // "audio",

    // Risk of listening attack.
    // "audioCapture",

    // Just resource management, probably doesn't even apply to Chrome OS.
    "background",

    // Open a new tab with a given URL.
    "browser",

    // Risky: Reading from clipboard could expose private information.
    // "clipboardRead",

    // Writing to clipboard is safe.
    "clipboardWrite",

    // Potentially risky: Could be used to spoof system UI.
    // "contextMenus",

    // Dev channel only.  Not evaluated.
    // "copresence",

    // Placing a document on the scanner implies user consent.
    "documentScan",

    // Possibly risky due to its experimental nature: not vetted for security,
    // potentially buggy, subject to change without notice.
    // "experimental",

    // Fullscreen is a no-op for Public Session.  Whitelisting nevertheless to
    // broaden the range of supported apps.  (The recommended permission names
    // are "app.window.*" but their unprefixed counterparts are still
    // supported.)
    "app.window.fullscreen",
    "app.window.fullscreen.overrideEsc",
    "fullscreen",
    "overrideEscFullscreen",

    // TBD
    // "fileSystemProvider",

    // Just another type of connectivity.  On the system side, no user data is
    // involved, implicitly or explicity.
    "gcm",

    // Risky: Accessing location without explicit user consent.
    // "geolocation",

    // Risky: Potentially allows keylogging.
    // "hid",

    // Detection of idle state.
    "idle",

    // Dev channel only.  Not evaluated.
    // "location",

    // Just another type of connectivity.
    "mdns",

    // Risky: The "allAutoDectected" option could allow access to user data
    // without their consent.
    // "mediaGalleries",

    // Potentially risky: Could be used to spoof system UI.
    // "notifications",

    // TBD.  Could allow UX spoofing.
    // "pointerLock",

    // Potentiall risky: chrome.power.requestKeepAwake can inhibit idle time
    // detection and prevent idle time logout and that way reduce isolation
    // between subsequent Public Session users.
    // "power",

    // Risky: Could be used to siphon printed documents.
    // "printerProvider",

    // Access serial port.  It's hard to conceive a case in which private data
    // is stored on a serial device and being read without the user's consent.
    "serial",

    // Per-app sandbox.  User cannot log into Public Session, thus storage
    // cannot be sync'ed to the cloud.
    "storage",

    // Access system parameters.
    "system.cpu",

    // Access system parameters.
    "system.display",

    // Access system parameters.
    "system.memory",

    // Access system parameters.
    "system.network",

    // Risky: Could leak the name of a user-supplied storage medium.
    // "system.storage",

    // Just UX.
    "tts",

    // Excessive resource usage is not a risk.
    "unlimitedStorage",

    // Risky: Raw peripheral access could allow an app to read user data from
    // USB storage devices that have been plugged in by the user.  Not sure if
    // that can happen though, because the system might claim storage devices
    // for itself.  Still, leaving disallowed for now to be on the safe side.
    // "usb",

    // TBD: What if one user connects and the next one is unaware of that?
    // "vpnProvider",

    // Just UX.
    "wallpaper",

    // Web capabilities are safe.
    "webview",
};

// Some permissions take the form of a dictionary.  See |kSafePermissionStrings|
// for permission strings (and for more documentation).
const char* const kSafePermissionDicts[] = {
    // TBD
    // "fileSystem",

    // Just another type of connectivity.
    "socket",
};

// Return true iff |entry| is contained in |char_array|.
bool ArrayContainsImpl(const char* const char_array[],
                       size_t entry_count,
                       const std::string& entry) {
  for (size_t i = 0; i < entry_count; ++i) {
    if (entry == char_array[i]) {
      return true;
    }
  }
  return false;
}

// See http://blogs.msdn.com/b/the1/archive/2004/05/07/128242.aspx for an
// explanation of array size determination.
template <size_t N>
bool ArrayContains(const char* const (&char_array)[N],
                   const std::string& entry) {
  return ArrayContainsImpl(char_array, N, entry);
}

// Returns true for platform apps that are considered safe for Public Sessions,
// which among other things requires the manifest top-level entries to be
// contained in the |kSafeManifestEntries| whitelist and all permissions to be
// contained in |kSafePermissionStrings| or |kSafePermissionDicts|.
bool IsPlatformAppSafeForPublicSession(const extensions::Extension* extension) {
  if (extension->GetType() != extensions::Manifest::TYPE_PLATFORM_APP) {
    LOG(ERROR) << extension->id() << " is not a platform app.";
    return false;
  }

  for (base::DictionaryValue::Iterator it(*extension->manifest()->value());
       !it.IsAtEnd(); it.Advance()) {
    if (ArrayContains(kSafeManifestEntries, it.key())) {
      continue;
    }

    // Permissions must be whitelisted in |kSafePermissionStrings| or
    // |kSafePermissionDicts|.
    if (it.key() == emk::kPermissions ||
        it.key() == emk::kOptionalPermissions) {
      const base::ListValue* list_value;
      if (!it.value().GetAsList(&list_value)) {
        LOG(ERROR) << extension->id() << ": " << it.key() << " is not a list.";
        return false;
      }
      for (auto it2 = list_value->begin(); it2 != list_value->end(); ++it2) {
        // Try to read as dictionary.
        const base::DictionaryValue *dict_value;
        if ((*it2)->GetAsDictionary(&dict_value)) {
          if (dict_value->size() != 1) {
            LOG(ERROR) << extension->id()
                       << " has dict in permission list with size "
                       << dict_value->size() << ".";
            return false;
          }
          for (base::DictionaryValue::Iterator it3(*dict_value);
               !it3.IsAtEnd(); it3.Advance()) {
            if (!ArrayContains(kSafePermissionDicts, it3.key())) {
              LOG(ERROR) << extension->id()
                         << " has non-whitelisted dict in permission list: "
                         << it3.key();
              return false;
            }
          }
          continue;
        }
        // Try to read as string.
        std::string permission_string;
        if (!(*it2)->GetAsString(&permission_string)) {
          LOG(ERROR) << extension->id() << ": " << it.key()
                     << " contains a token that's neither a string nor a dict.";
          return false;
        }
        // Accept whitelisted permissions.
        if (ArrayContains(kSafePermissionStrings, permission_string)) {
          continue;
        }
        // Allow arbitrary web requests.  Don't include <all_urls> because that
        // also matches file:// schemes.
        if (permission_string.find("https://") == 0 ||
            permission_string.find("http://") == 0 ||
            permission_string.find("ftp://") == 0) {
          continue;
        }
        LOG(ERROR) << extension->id()
                   << " requested non-whitelisted permission: "
                   << permission_string;
        return false;
      }
    // "app" may only contain "background".
    } else if (it.key() == emk::kApp) {
      const base::DictionaryValue *dict_value;
      if (!it.value().GetAsDictionary(&dict_value)) {
        LOG(ERROR) << extension->id() << ": app is not a dictionary.";
        return false;
      }
      for (base::DictionaryValue::Iterator it2(*dict_value);
           !it2.IsAtEnd(); it2.Advance()) {
        if (it2.key() != "background") {
          LOG(ERROR) << extension->id()
                     << " has non-whitelisted manifest entry: "
                     << it.key() << "." << it2.key();
          return false;
        }
      }
    // Require v2 because that's the only version we understand.
    } else if (it.key() == emk::kManifestVersion) {
      int version;
      if (!it.value().GetAsInteger(&version)) {
        LOG(ERROR) << extension->id() << ": " << emk::kManifestVersion
                   << " is not an integer.";
        return false;
      }
      if (version != 2) {
        LOG(ERROR) << extension->id()
                   << " has non-whitelisted manifest version.";
        return false;
      }
    // URL handlers depend on the web store to confirm ownership of the domain.
    } else if (it.key() == emk::kUrlHandlers) {
      if (!extension->from_webstore()) {
        LOG(ERROR) << extension->id() << " uses emk::kUrlHandlers but was not "
            "installed through the web store.";
        return false;
      }
    // Everything else is an error.
    } else {
      LOG(ERROR) << extension->id()
                 << " has non-whitelisted manifest entry: " << it.key();
      return false;
    }
  }

  return true;
}

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
    if (extension->location() == extensions::Manifest::EXTERNAL_COMPONENT) {
      return true;
    }

    // Allow extension if its type is whitelisted for use in public sessions.
    if (extension->GetType() == extensions::Manifest::TYPE_HOSTED_APP) {
      return true;
    }

    // Allow extension if its specific ID is whitelisted for use in public
    // sessions.
    if (ArrayContains(kPublicSessionWhitelist, extension->id())) {
      return true;
    }

    // Allow force-installed platform app if all manifest contents are
    // whitelisted.
    if ((extension->location() == extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD
         || extension->location() == extensions::Manifest::EXTERNAL_POLICY)
        && IsPlatformAppSafeForPublicSession(extension)) {
      return true;
    }
  } else if (account_type_ == policy::DeviceLocalAccount::TYPE_KIOSK_APP) {
    // For single-app kiosk sessions, allow platform apps, extesions and shared
    // modules.
    if (extension->GetType() == extensions::Manifest::TYPE_PLATFORM_APP ||
        extension->GetType() == extensions::Manifest::TYPE_SHARED_MODULE ||
        extension->GetType() == extensions::Manifest::TYPE_EXTENSION) {
      return true;
    }
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
