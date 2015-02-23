// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"

#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/permissions/api_permission_set.h"
#include "extensions/common/permissions/coalesced_permission_message.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

namespace {

// The default formatter for a permission message. Simply displays the message
// with the given ID.
class DefaultPermissionMessageFormatter
    : public ChromePermissionMessageFormatter {
 public:
  explicit DefaultPermissionMessageFormatter(int message_id)
      : message_id_(message_id) {}
  ~DefaultPermissionMessageFormatter() override {}

  CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const override {
    return CoalescedPermissionMessage(l10n_util::GetStringUTF16(message_id_),
                                      permissions);
  }

 private:
  int message_id_;

  // DISALLOW_COPY_AND_ASSIGN(DefaultPermissionMessageFormatter);
};

// A formatter that substitutes the parameter into the message using string
// formatting.
// NOTE: Only one permission with the given ID is substituted using this rule.
class SingleParameterFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SingleParameterFormatter(int message_id) : message_id_(message_id) {}
  ~SingleParameterFormatter() override {}

  CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<base::string16> parameters =
        permissions.GetAllPermissionParameters();
    DCHECK_EQ(1U, parameters.size())
        << "Only one message with each ID can be parameterized.";
    return CoalescedPermissionMessage(
        l10n_util::GetStringFUTF16(message_id_, parameters[0]), permissions);
  }

 private:
  int message_id_;
};

// Adds each parameter to a growing list, with the given |root_message_id| as
// the message at the top of the list.
class SimpleListFormatter : public ChromePermissionMessageFormatter {
 public:
  explicit SimpleListFormatter(int root_message_id)
      : root_message_id_(root_message_id) {}
  ~SimpleListFormatter() override {}

  CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const override {
    DCHECK(permissions.size() > 0);
    return CoalescedPermissionMessage(
        l10n_util::GetStringUTF16(root_message_id_), permissions,
        permissions.GetAllPermissionParameters());
  }

 private:
  int root_message_id_;
};

// Creates a space-separated list of permissions with the given PermissionID.
// The list is inserted into the messages with the given IDs: one for the case
// where there is a single permission, and the other for the case where there
// are multiple.
// TODO(sashab): Extend this to pluralize correctly in all languages.
class SpaceSeparatedListFormatter : public ChromePermissionMessageFormatter {
 public:
  SpaceSeparatedListFormatter(int message_id_for_one_host,
                              int message_id_for_multiple_hosts)
      : message_id_for_one_host_(message_id_for_one_host),
        message_id_for_multiple_hosts_(message_id_for_multiple_hosts) {}
  ~SpaceSeparatedListFormatter() override {}

  CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<base::string16> hostnames =
        permissions.GetAllPermissionParameters();
    base::string16 hosts_string = JoinString(
        std::vector<base::string16>(hostnames.begin(), hostnames.end()), ' ');
    return CoalescedPermissionMessage(
        l10n_util::GetStringFUTF16(hostnames.size() == 1
                                       ? message_id_for_one_host_
                                       : message_id_for_multiple_hosts_,
                                   hosts_string),
        permissions);
  }

 private:
  int message_id_for_one_host_;
  int message_id_for_multiple_hosts_;
};

// Creates a comma-separated list of permissions with the given PermissionID.
// The list is inserted into the messages with the given IDs: one for each case
// of 1-3 permissions, and the other for the case where there are 4 or more
// permissions. In the case of 4 or more permissions, rather than insert the
// list into the message, the permissions are displayed as submessages in the
// resultant CoalescedPermissionMessage.
class CommaSeparatedListFormatter : public ChromePermissionMessageFormatter {
 public:
  CommaSeparatedListFormatter(int message_id_for_one_host,
                              int message_id_for_two_hosts,
                              int message_id_for_three_hosts,
                              int message_id_for_many_hosts)
      : message_id_for_one_host_(message_id_for_one_host),
        message_id_for_two_hosts_(message_id_for_two_hosts),
        message_id_for_three_hosts_(message_id_for_three_hosts),
        message_id_for_many_hosts_(message_id_for_many_hosts) {}
  ~CommaSeparatedListFormatter() override {}

  CoalescedPermissionMessage GetPermissionMessage(
      PermissionIDSet permissions) const override {
    DCHECK(permissions.size() > 0);
    std::vector<base::string16> hostnames =
        permissions.GetAllPermissionParameters();
    CoalescedPermissionMessages messages;
    if (hostnames.size() <= 3) {
      return CoalescedPermissionMessage(
          l10n_util::GetStringFUTF16(message_id_for_hosts(hostnames.size()),
                                     hostnames, NULL),
          permissions);
    }

    return CoalescedPermissionMessage(
        l10n_util::GetStringUTF16(message_id_for_many_hosts_), permissions,
        hostnames);
  }

 private:
  int message_id_for_hosts(int number_of_hosts) const {
    switch (number_of_hosts) {
      case 1:
        return message_id_for_one_host_;
      case 2:
        return message_id_for_two_hosts_;
      case 3:
        return message_id_for_three_hosts_;
      default:
        return message_id_for_many_hosts_;
    }
  }

  int message_id_for_one_host_;
  int message_id_for_two_hosts_;
  int message_id_for_three_hosts_;
  int message_id_for_many_hosts_;
};

}  // namespace

ChromePermissionMessageRule::ChromePermissionMessageRule(
    int message_id,
    PermissionIDSetInitializer required,
    PermissionIDSetInitializer optional)
    : required_permissions_(required),
      optional_permissions_(optional),
      formatter_(new DefaultPermissionMessageFormatter(message_id)) {
}

ChromePermissionMessageRule::ChromePermissionMessageRule(
    ChromePermissionMessageFormatter* formatter,
    PermissionIDSetInitializer required,
    PermissionIDSetInitializer optional)
    : required_permissions_(required),
      optional_permissions_(optional),
      formatter_(formatter) {
}

ChromePermissionMessageRule::~ChromePermissionMessageRule() {
}

std::set<APIPermission::ID> ChromePermissionMessageRule::required_permissions()
    const {
  return required_permissions_;
}
std::set<APIPermission::ID> ChromePermissionMessageRule::optional_permissions()
    const {
  return optional_permissions_;
}
ChromePermissionMessageFormatter* ChromePermissionMessageRule::formatter()
    const {
  return formatter_.get();
}

std::set<APIPermission::ID> ChromePermissionMessageRule::all_permissions()
    const {
  return base::STLSetUnion<std::set<APIPermission::ID>>(required_permissions(),
                                                        optional_permissions());
}

// static
std::vector<ChromePermissionMessageRule>
ChromePermissionMessageRule::GetAllRules() {
  // The rules for generating messages from permissions. Any new rules should be
  // added directly to this list, not elsewhere in the code, so that all the
  // logic of generating and coalescing permission messages happens here.
  //
  // Each rule has 3 components:
  // 1. The message itself
  // 2. The permissions that need to be present for the message to appear
  // 3. Permissions that, if present, also contribute to the message, but do not
  //    form the message on their own
  //
  // Rules are applied in precedence order: rules that come first consume
  // permissions (both required and optional) so they can not be used in later
  // rules.
  // NOTE: The order of this list matters - be careful when adding new rules!
  // If unsure, add them near related rules and add tests to
  // permission_message_combinations_unittest.cc (or elsewhere) to ensure your
  // messages are being generated/coalesced correctly.
  //
  // Rules are not transitive: This means that if the kTab permission 'absorbs'
  // (suppresses) the messages for kTopSites and kFavicon, and the kHistory
  // permission suppresses kTab, be careful to also add kTopSites and kFavicon
  // to the kHistory absorb list. Ideally, the rules system should be simple
  // enough that rules like this should not occur; the visibility of the rules
  // system should allow us to design a system that is simple enough to explain
  // yet powerful enough to encapsulate all the messages we want to display.
  //
  // TODO(sashab): Once existing message sites are deprecated, reorder this list
  // to better describe the rules generated, rather than the callsites they are
  // migrated from.
  ChromePermissionMessageRule rules_arr[] = {
      // Full url access permission messages.
      {IDS_EXTENSION_PROMPT_WARNING_DEBUGGER, {APIPermission::kDebugger}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
       {APIPermission::kPlugin},
       {APIPermission::kFullAccess,
        APIPermission::kHostsAll,
        APIPermission::kHostsAllReadOnly,
        APIPermission::kDeclarativeWebRequest,
        APIPermission::kTopSites,
        APIPermission::kTab}},
      {IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS,
       {APIPermission::kFullAccess},
       {APIPermission::kHostsAll,
        APIPermission::kHostsAllReadOnly,
        APIPermission::kDeclarativeWebRequest,
        APIPermission::kTopSites,
        APIPermission::kTab}},

      // Parameterized permission messages:
      // Messages generated by the sockets permission.
      {new SpaceSeparatedListFormatter(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOST,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_SPECIFIC_HOSTS),
       {APIPermission::kSocketSpecificHosts},
       {}},
      {new SpaceSeparatedListFormatter(
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAIN,
           IDS_EXTENSION_PROMPT_WARNING_SOCKET_HOSTS_IN_DOMAINS),
       {APIPermission::kSocketDomainHosts},
       {}},

      // Messages generated by host permissions.
      {new CommaSeparatedListFormatter(
           IDS_EXTENSION_PROMPT_WARNING_1_HOST_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_2_HOSTS_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_3_HOSTS_READ_ONLY,
           IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST_READ_ONLY),
       {APIPermission::kHostReadOnly},
       {}},
      {new CommaSeparatedListFormatter(IDS_EXTENSION_PROMPT_WARNING_1_HOST,
                                       IDS_EXTENSION_PROMPT_WARNING_2_HOSTS,
                                       IDS_EXTENSION_PROMPT_WARNING_3_HOSTS,
                                       IDS_EXTENSION_PROMPT_WARNING_HOSTS_LIST),
       {APIPermission::kHostReadWrite},
       {}},

      // USB Device Permission rules:
      // TODO(sashab, reillyg): Rework the permission message logic for USB
      // devices to generate more meaningful messages and better fit the current
      // rules system. Maybe model it similarly to host or socket permissions
      // above.
      {new SingleParameterFormatter(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE),
       {APIPermission::kUsbDevice},
       {}},
      {new SingleParameterFormatter(
           IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_PRODUCT),
       {APIPermission::kUsbDeviceUnknownProduct},
       {}},
      {new SimpleListFormatter(IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_LIST),
       {APIPermission::kUsbDeviceList},
       {}},

      // Coalesced message rules taken from
      // ChromePermissionMessageProvider::GetWarningMessages():

      // Access to users' devices should provide a single warning message
      // specifying the transport method used; serial and/or Bluetooth.
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_SERIAL,
       {APIPermission::kBluetooth, APIPermission::kSerial},
       {APIPermission::kBluetoothDevices}},

      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ_MODIFY,
       {APIPermission::kAccessibilityFeaturesModify,
        APIPermission::kAccessibilityFeaturesRead},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE,
       {APIPermission::kAudioCapture, APIPermission::kVideoCapture},
       {}},

      // TODO(sashab): Add the missing combinations of media galleries
      // permissions so a valid permission is generated for all combinations.
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE_DELETE,
       {APIPermission::kMediaGalleriesAllGalleriesCopyTo,
        APIPermission::kMediaGalleriesAllGalleriesDelete,
        APIPermission::kMediaGalleriesAllGalleriesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE,
       {APIPermission::kMediaGalleriesAllGalleriesCopyTo,
        APIPermission::kMediaGalleriesAllGalleriesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_DELETE,
       {APIPermission::kMediaGalleriesAllGalleriesDelete,
        APIPermission::kMediaGalleriesAllGalleriesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ,
       {APIPermission::kMediaGalleriesAllGalleriesRead},
       {}},

      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE_AND_SESSIONS,
       {APIPermission::kSessions, APIPermission::kHistory},
       {APIPermission::kTab,
        APIPermission::kFavicon,
        APIPermission::kTopSites}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_AND_SESSIONS,
       {APIPermission::kSessions, APIPermission::kTab},
       {APIPermission::kFavicon, APIPermission::kTopSites}},

      // Suppression list taken from
      // ChromePermissionMessageProvider::GetPermissionMessages():
      // Some warnings are more generic and/or powerful and supercede other
      // warnings. In that case, the first message suppresses the second one.
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH,
       {APIPermission::kBluetooth},
       {APIPermission::kBluetoothDevices}},
      {IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS,
       {APIPermission::kBookmark},
       {APIPermission::kOverrideBookmarksUI}},
      // History already allows reading favicons, tab access and accessing the
      // list of most frequently visited sites.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE,
       {APIPermission::kHistory},
       {APIPermission::kFavicon,
        APIPermission::kTab,
        APIPermission::kFavicon,
        APIPermission::kTopSites}},
      // A special hack: If kFileSystemWriteDirectory would be displayed, hide
      // kFileSystemDirectory as the write directory message implies it.
      // TODO(sashab): Remove kFileSystemWriteDirectory; it's no longer needed
      // since this rules system can represent the rule. See crbug.com/284849.
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE_DIRECTORY,
       {APIPermission::kFileSystemWrite, APIPermission::kFileSystemDirectory},
       {APIPermission::kFileSystemWriteDirectory}},
      // Full access already allows DeclarativeWebRequest, reading the list of
      // most frequently visited sites, and tab access.
      // The warning message for declarativeWebRequest
      // permissions speaks about blocking parts of pages, which is a
      // subset of what the "<all_urls>" access allows. Therefore we
      // display only the "<all_urls>" warning message if both permissions
      // are required.
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS,
       {APIPermission::kHostsAll},
       {APIPermission::kDeclarativeWebRequest,
        APIPermission::kTopSites,
        APIPermission::kTab,
        APIPermission::kFavicon,
        APIPermission::kTopSites,
        APIPermission::kHostsAllReadOnly}},
      // AutomationManifestPermission:
      {IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS_READ_ONLY,
       {APIPermission::kHostsAllReadOnly},
       {}},
      // Tabs already allows reading favicons and reading the list of most
      // frequently visited sites.
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kTab},
       {APIPermission::kFavicon, APIPermission::kTopSites}},

      // Individual message rules taken from
      // ChromeAPIPermissions::GetAllPermissions():
      // Permission messages for all extension types:

      {IDS_EXTENSION_PROMPT_WARNING_CLIPBOARD,
       {APIPermission::kClipboardRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DESKTOP_CAPTURE,
       {APIPermission::kDesktopCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS, {APIPermission::kDownloads}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOWNLOADS_OPEN,
       {APIPermission::kDownloadsOpen},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_IDENTITY_EMAIL,
       {APIPermission::kIdentityEmail},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
       {APIPermission::kGeolocation},
       {}},

      // Permission messages for extensions:
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_MODIFY,
       {APIPermission::kAccessibilityFeaturesModify},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ,
       {APIPermission::kAccessibilityFeaturesRead},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_BOOKMARKS, {APIPermission::kBookmark}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_CONTENT_SETTINGS,
       {APIPermission::kContentSettings},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_COPRESENCE,
       {APIPermission::kCopresence},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DOCUMENT_SCAN,
       {APIPermission::kDocumentScan},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE,
       {APIPermission::kHistory},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_INPUT, {APIPermission::kInput}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_GEOLOCATION,
       {APIPermission::kLocation},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MANAGEMENT,
       {APIPermission::kManagement},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NATIVE_MESSAGING,
       {APIPermission::kNativeMessaging},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_PRIVACY, {APIPermission::kPrivacy}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kProcesses},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SIGNED_IN_DEVICES,
       {APIPermission::kSignedInDevices},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SYNCFILESYSTEM,
       {APIPermission::kSyncFileSystem},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ, {APIPermission::kTab}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_TOPSITES, {APIPermission::kTopSites}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_TTS_ENGINE,
       {APIPermission::kTtsEngine},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_WALLPAPER, {APIPermission::kWallpaper}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ,
       {APIPermission::kWebNavigation},
       {}},

      // Permission messages for private permissions:
      {IDS_EXTENSION_PROMPT_WARNING_SCREENLOCK_PRIVATE,
       {APIPermission::kScreenlockPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_ACTIVITY_LOG_PRIVATE,
       {APIPermission::kActivityLogPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_EXPERIENCE_SAMPLING_PRIVATE,
       {APIPermission::kExperienceSamplingPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORKING_PRIVATE,
       {APIPermission::kNetworkingPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_MUSIC_MANAGER_PRIVATE,
       {APIPermission::kMusicManagerPrivate},
       {}},

      // Platform-app permission messages.

      // The permission string for "fileSystem" is only shown when
      // "write" or "directory" is present. Read-only access is only
      // granted after the user has been shown a file or directory
      // chooser dialog and selected a file or directory. Selecting
      // the file or directory is considered consent to read it.
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_DIRECTORY,
       {APIPermission::kFileSystemDirectory},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_FILE_SYSTEM_WRITE_DIRECTORY,
       {APIPermission::kFileSystemWriteDirectory},
       {}},

      // Because warning messages for the "mediaGalleries" permission
      // vary based on the permissions parameters, no message ID or
      // message text is specified here.  The message ID and text used
      // will be determined at run-time in the
      // |MediaGalleriesPermission| class.

      {IDS_EXTENSION_PROMPT_WARNING_INTERCEPT_ALL_KEYS,
       {APIPermission::kInterceptAllKeys},
       {}},

      // Settings override permission messages.
      {IDS_EXTENSION_PROMPT_WARNING_HOME_PAGE_SETTING_OVERRIDE,
       {APIPermission::kHomepage},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SEARCH_SETTINGS_OVERRIDE,
       {APIPermission::kSearchProvider},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_START_PAGE_SETTING_OVERRIDE,
       {APIPermission::kStartupPages},
       {}},

      // Individual message rules taken from
      // ExtensionsAPIPermissions::GetAllPermissions():
      {IDS_EXTENSION_PROMPT_WARNING_AUDIO_CAPTURE,
       {APIPermission::kAudioCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_PRIVATE,
       {APIPermission::kBluetoothPrivate},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_DECLARATIVE_WEB_REQUEST,
       {APIPermission::kDeclarativeWebRequest},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SERIAL, {APIPermission::kSerial}, {}},

      // Because warning messages for the "socket" permission vary based
      // on the permissions parameters, no message ID or message text is
      // specified here.  The message ID and text used will be
      // determined at run-time in the |SocketPermission| class.
      {IDS_EXTENSION_PROMPT_WARNING_U2F_DEVICES,
       {APIPermission::kU2fDevices},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VIDEO_CAPTURE,
       {APIPermission::kVideoCapture},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_VPN, {APIPermission::kVpnProvider}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_WEB_CONNECTABLE,
       {APIPermission::kWebConnectable},
       {}},

      // Rules from ManifestPermissions:
      // BluetoothManifestPermission:
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH, {APIPermission::kBluetooth}, {}},
      {IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_DEVICES,
       {APIPermission::kBluetoothDevices},
       {}},

      // SocketsManifestPermission:
      {IDS_EXTENSION_PROMPT_WARNING_SOCKET_ANY_HOST,
       {APIPermission::kSocketAnyHost},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_NETWORK_STATE,
       {APIPermission::kNetworkState},
       {}},

      // API permission rules:
      // SettingsOverrideAPIPermission:
      {IDS_EXTENSION_PROMPT_WARNING_HOME_PAGE_SETTING_OVERRIDE,
       {APIPermission::kHomepage},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_START_PAGE_SETTING_OVERRIDE,
       {APIPermission::kStartupPages},
       {}},
      {IDS_EXTENSION_PROMPT_WARNING_SEARCH_SETTINGS_OVERRIDE,
       {APIPermission::kSearchProvider},
       {}},

      // USBDevicePermission:
      {IDS_EXTENSION_PROMPT_WARNING_USB_DEVICE_UNKNOWN_VENDOR,
       {APIPermission::kUsbDeviceUnknownVendor},
       {}},

      // Other rules:
      // From ChromeExtensionsClient::FilterHostPermissions():
      {IDS_EXTENSION_PROMPT_WARNING_FAVICON, {APIPermission::kFavicon}, {}},
  };

  std::vector<ChromePermissionMessageRule> rules;
  for (size_t i = 0; i < arraysize(rules_arr); i++) {
    rules.push_back(rules_arr[i]);
  }
  return rules;
}

ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer() {
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one) {
  insert(permission_one);
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two) {
  insert(permission_one);
  insert(permission_two);
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three) {
  insert(permission_one);
  insert(permission_two);
  insert(permission_three);
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four) {
  insert(permission_one);
  insert(permission_two);
  insert(permission_three);
  insert(permission_four);
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four,
                               APIPermission::ID permission_five) {
  insert(permission_one);
  insert(permission_two);
  insert(permission_three);
  insert(permission_four);
  insert(permission_five);
}
ChromePermissionMessageRule::PermissionIDSetInitializer::
    PermissionIDSetInitializer(APIPermission::ID permission_one,
                               APIPermission::ID permission_two,
                               APIPermission::ID permission_three,
                               APIPermission::ID permission_four,
                               APIPermission::ID permission_five,
                               APIPermission::ID permission_six) {
  insert(permission_one);
  insert(permission_two);
  insert(permission_three);
  insert(permission_four);
  insert(permission_five);
  insert(permission_six);
}

ChromePermissionMessageRule::PermissionIDSetInitializer::
    ~PermissionIDSetInitializer() {
}

}  // namespace extensions
