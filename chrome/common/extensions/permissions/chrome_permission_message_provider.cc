// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/permissions/permission_message.h"
#include "extensions/common/permissions/permission_message_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

namespace {

typedef std::set<PermissionMessage> PermissionMsgSet;

template<typename T>
typename T::iterator FindMessageByID(T& messages, int id) {
  for (typename T::iterator it = messages.begin();
       it != messages.end(); ++it) {
    if (it->id() == id)
      return it;
  }
  return messages.end();
}

template<typename T>
typename T::const_iterator FindMessageByID(const T& messages, int id) {
  for (typename T::const_iterator it = messages.begin();
       it != messages.end(); ++it) {
    if (it->id() == id)
      return it;
  }
  return messages.end();
}

template<typename T>
void SuppressMessage(T& messages,
                     int suppressing_message,
                     int suppressed_message) {
  typename T::iterator suppressed = FindMessageByID(messages,
                                                    suppressed_message);
  if (suppressed != messages.end() &&
      FindMessageByID(messages, suppressing_message) != messages.end()) {
    messages.erase(suppressed);
  }
}

bool ContainsMessages(const PermissionMessages& messages,
                      int first_message,
                      int second_message) {
  return FindMessageByID(messages, first_message) != messages.end() &&
         FindMessageByID(messages, second_message) != messages.end();
}

bool ContainsMessages(const PermissionMessages& messages,
                      int first_message,
                      int second_message,
                      int third_message) {
  return ContainsMessages(messages, first_message, second_message) &&
         FindMessageByID(messages, third_message) != messages.end();
}

}  // namespace

ChromePermissionMessageProvider::ChromePermissionMessageProvider() {
}

ChromePermissionMessageProvider::~ChromePermissionMessageProvider() {
}

PermissionMessages ChromePermissionMessageProvider::GetPermissionMessages(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  // Some warnings are more generic and/or powerful and superseed other
  // warnings. In that case, the first message suppresses the second one.
  std::multimap<PermissionMessage::ID, PermissionMessage::ID> kSuppressList;
  kSuppressList.insert(
      {PermissionMessage::kBluetooth, PermissionMessage::kBluetoothDevices});
  kSuppressList.insert(
      {PermissionMessage::kBookmarks, PermissionMessage::kOverrideBookmarksUI});
  // History already allows reading favicons.
  kSuppressList.insert(
      {PermissionMessage::kBrowsingHistory, PermissionMessage::kFavicon});
  // History already allows tabs access.
  kSuppressList.insert(
      {PermissionMessage::kBrowsingHistory, PermissionMessage::kTabs});
  // A special hack: If kFileSystemWriteDirectory would be displayed, hide
  // kFileSystemDirectory as the write directory message implies it.
  // TODO(sammc): Remove this. See http://crbug.com/284849.
  kSuppressList.insert({PermissionMessage::kFileSystemWriteDirectory,
                        PermissionMessage::kFileSystemDirectory});
  // Full access already allows DeclarativeWebRequest.
  kSuppressList.insert({PermissionMessage::kHostsAll,
                        PermissionMessage::kDeclarativeWebRequest});
  // Full access already covers tabs access.
  kSuppressList.insert(
      {PermissionMessage::kHostsAll, PermissionMessage::kTabs});
  // Tabs already allows reading favicons.
  kSuppressList.insert({PermissionMessage::kTabs, PermissionMessage::kFavicon});

  PermissionMessages messages;

  if (permissions->HasEffectiveFullAccess()) {
    messages.push_back(PermissionMessage(
        PermissionMessage::kFullAccess,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_FULL_ACCESS)));
    return messages;
  }

  PermissionMsgSet host_msgs =
      GetHostPermissionMessages(permissions, extension_type);
  PermissionMsgSet api_msgs = GetAPIPermissionMessages(permissions);
  PermissionMsgSet manifest_permission_msgs =
      GetManifestPermissionMessages(permissions);
  messages.insert(messages.end(), host_msgs.begin(), host_msgs.end());
  messages.insert(messages.end(), api_msgs.begin(), api_msgs.end());
  messages.insert(messages.end(), manifest_permission_msgs.begin(),
                  manifest_permission_msgs.end());

  for (std::multimap<PermissionMessage::ID,
                     PermissionMessage::ID>::const_iterator it =
           kSuppressList.begin();
       it != kSuppressList.end();
       ++it) {
    SuppressMessage(messages, it->first, it->second);
  }

  return messages;
}

std::vector<base::string16> ChromePermissionMessageProvider::GetWarningMessages(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  std::vector<base::string16> message_strings;
  PermissionMessages messages =
      GetPermissionMessages(permissions, extension_type);

  for (PermissionMessages::const_iterator i = messages.begin();
       i != messages.end(); ++i) {
    int id = i->id();
    // Access to users' devices should provide a single warning message
    // specifying the transport method used; USB, serial and/or Bluetooth.
    if (id == PermissionMessage::kBluetooth ||
        id == PermissionMessage::kSerial ||
        id == PermissionMessage::kUsb) {
      if (ContainsMessages(messages,
                           PermissionMessage::kBluetooth,
                           PermissionMessage::kSerial,
                           PermissionMessage::kUsb)) {
        if (id == PermissionMessage::kBluetooth) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_ALL_DEVICES));
        }
        continue;
      }
      if (ContainsMessages(messages,
                           PermissionMessage::kBluetooth,
                           PermissionMessage::kUsb)) {
        if (id == PermissionMessage::kBluetooth) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_BLUETOOTH));
        }
        continue;
      }
      if (ContainsMessages(messages,
                           PermissionMessage::kSerial,
                           PermissionMessage::kUsb)) {
        if (id == PermissionMessage::kSerial) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_USB_SERIAL));
        }
        continue;
      }
      if (ContainsMessages(messages,
                           PermissionMessage::kBluetooth,
                           PermissionMessage::kSerial)) {
        if (id == PermissionMessage::kBluetooth) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_BLUETOOTH_SERIAL));
        }
        continue;
      }
    }
    if (id == PermissionMessage::kAccessibilityFeaturesModify ||
        id == PermissionMessage::kAccessibilityFeaturesRead) {
      if (ContainsMessages(messages,
                           PermissionMessage::kAccessibilityFeaturesModify,
                           PermissionMessage::kAccessibilityFeaturesRead)) {
        if (id == PermissionMessage::kAccessibilityFeaturesModify) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_ACCESSIBILITY_FEATURES_READ_MODIFY));
        }
        continue;
      }
    }
    if (id == PermissionMessage::kAudioCapture ||
        id == PermissionMessage::kVideoCapture) {
      if (ContainsMessages(messages,
                           PermissionMessage::kAudioCapture,
                           PermissionMessage::kVideoCapture)) {
        if (id == PermissionMessage::kAudioCapture) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_AUDIO_AND_VIDEO_CAPTURE));
        }
        continue;
      }
    }
    if (id == PermissionMessage::kMediaGalleriesAllGalleriesCopyTo ||
        id == PermissionMessage::kMediaGalleriesAllGalleriesDelete ||
        id == PermissionMessage::kMediaGalleriesAllGalleriesRead) {
      if (ContainsMessages(
              messages,
              PermissionMessage::kMediaGalleriesAllGalleriesCopyTo,
              PermissionMessage::kMediaGalleriesAllGalleriesDelete,
              PermissionMessage::kMediaGalleriesAllGalleriesRead)) {
        if (id == PermissionMessage::kMediaGalleriesAllGalleriesCopyTo) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE_DELETE));
        }
        continue;
      }
      if (ContainsMessages(
              messages,
              PermissionMessage::kMediaGalleriesAllGalleriesCopyTo,
              PermissionMessage::kMediaGalleriesAllGalleriesRead)) {
        if (id == PermissionMessage::kMediaGalleriesAllGalleriesCopyTo) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE));
        }
        continue;
      }
      if (ContainsMessages(
              messages,
              PermissionMessage::kMediaGalleriesAllGalleriesDelete,
              PermissionMessage::kMediaGalleriesAllGalleriesRead)) {
        if (id == PermissionMessage::kMediaGalleriesAllGalleriesDelete) {
          message_strings.push_back(l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_DELETE));
        }
        continue;
      }
    }
    if (permissions->HasAPIPermission(APIPermission::kSessions) &&
        id == PermissionMessage::kTabs) {
      message_strings.push_back(l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_HISTORY_READ_AND_SESSIONS));
      continue;
    }
    if (permissions->HasAPIPermission(APIPermission::kSessions) &&
        id == PermissionMessage::kBrowsingHistory) {
      message_strings.push_back(l10n_util::GetStringUTF16(
          IDS_EXTENSION_PROMPT_WARNING_HISTORY_WRITE_AND_SESSIONS));
      continue;
    }

    message_strings.push_back(i->message());
  }

  return message_strings;
}

std::vector<base::string16>
ChromePermissionMessageProvider::GetWarningMessagesDetails(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  std::vector<base::string16> message_strings;
  PermissionMessages messages =
      GetPermissionMessages(permissions, extension_type);

  for (PermissionMessages::const_iterator i = messages.begin();
       i != messages.end(); ++i)
    message_strings.push_back(i->details());

  return message_strings;
}

bool ChromePermissionMessageProvider::IsPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions,
    Manifest::Type extension_type) const {
  // Things can't get worse than native code access.
  if (old_permissions->HasEffectiveFullAccess())
    return false;

  // Otherwise, it's a privilege increase if the new one has full access.
  if (new_permissions->HasEffectiveFullAccess())
    return true;

  if (IsHostPrivilegeIncrease(old_permissions, new_permissions, extension_type))
    return true;

  if (IsAPIPrivilegeIncrease(old_permissions, new_permissions))
    return true;

  if (IsManifestPermissionPrivilegeIncrease(old_permissions, new_permissions))
    return true;

  return false;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetAPIPermissionMessages(
    const PermissionSet* permissions) const {
  PermissionMsgSet messages;
  for (APIPermissionSet::const_iterator permission_it =
           permissions->apis().begin();
       permission_it != permissions->apis().end(); ++permission_it) {
    if (permission_it->HasMessages()) {
      PermissionMessages new_messages = permission_it->GetMessages();
      messages.insert(new_messages.begin(), new_messages.end());
    }
  }

  // A special hack: The warning message for declarativeWebRequest
  // permissions speaks about blocking parts of pages, which is a
  // subset of what the "<all_urls>" access allows. Therefore we
  // display only the "<all_urls>" warning message if both permissions
  // are required.
  if (permissions->ShouldWarnAllHosts()) {
    messages.erase(
        PermissionMessage(
            PermissionMessage::kDeclarativeWebRequest, base::string16()));
  }
  return messages;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetManifestPermissionMessages(
    const PermissionSet* permissions) const {
  PermissionMsgSet messages;
  for (ManifestPermissionSet::const_iterator permission_it =
           permissions->manifest_permissions().begin();
      permission_it != permissions->manifest_permissions().end();
      ++permission_it) {
    if (permission_it->HasMessages()) {
      PermissionMessages new_messages = permission_it->GetMessages();
      messages.insert(new_messages.begin(), new_messages.end());
    }
  }
  return messages;
}

std::set<PermissionMessage>
ChromePermissionMessageProvider::GetHostPermissionMessages(
    const PermissionSet* permissions,
    Manifest::Type extension_type) const {
  PermissionMsgSet messages;
  // Since platform apps always use isolated storage, they can't (silently)
  // access user data on other domains, so there's no need to prompt.
  // Note: this must remain consistent with IsHostPrivilegeIncrease.
  // See crbug.com/255229.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return messages;

  if (permissions->ShouldWarnAllHosts()) {
    messages.insert(PermissionMessage(
        PermissionMessage::kHostsAll,
        l10n_util::GetStringUTF16(IDS_EXTENSION_PROMPT_WARNING_ALL_HOSTS)));
  } else {
    URLPatternSet regular_hosts;
    ExtensionsClient::Get()->FilterHostPermissions(
        permissions->effective_hosts(), &regular_hosts, &messages);

    std::set<std::string> hosts =
        permission_message_util::GetDistinctHosts(regular_hosts, true, true);
    if (!hosts.empty())
      messages.insert(permission_message_util::CreateFromHostList(hosts));
  }
  return messages;
}

bool ChromePermissionMessageProvider::IsAPIPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions) const {
  if (new_permissions == NULL)
    return false;

  PermissionMsgSet old_warnings = GetAPIPermissionMessages(old_permissions);
  PermissionMsgSet new_warnings = GetAPIPermissionMessages(new_permissions);
  PermissionMsgSet delta_warnings =
      base::STLSetDifference<PermissionMsgSet>(new_warnings, old_warnings);

  // A special hack: kFileSystemWriteDirectory implies kFileSystemDirectory.
  // TODO(sammc): Remove this. See http://crbug.com/284849.
  if (old_warnings.find(PermissionMessage(
          PermissionMessage::kFileSystemWriteDirectory, base::string16())) !=
      old_warnings.end()) {
    delta_warnings.erase(
        PermissionMessage(PermissionMessage::kFileSystemDirectory,
                          base::string16()));
  }

  // It is a privilege increase if there are additional warnings present.
  return !delta_warnings.empty();
}

bool ChromePermissionMessageProvider::IsManifestPermissionPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions) const {
  if (new_permissions == NULL)
    return false;

  PermissionMsgSet old_warnings =
      GetManifestPermissionMessages(old_permissions);
  PermissionMsgSet new_warnings =
      GetManifestPermissionMessages(new_permissions);
  PermissionMsgSet delta_warnings =
      base::STLSetDifference<PermissionMsgSet>(new_warnings, old_warnings);

  // It is a privilege increase if there are additional warnings present.
  return !delta_warnings.empty();
}

bool ChromePermissionMessageProvider::IsHostPrivilegeIncrease(
    const PermissionSet* old_permissions,
    const PermissionSet* new_permissions,
    Manifest::Type extension_type) const {
  // Platform apps host permission changes do not count as privilege increases.
  // Note: this must remain consistent with GetHostPermissionMessages.
  if (extension_type == Manifest::TYPE_PLATFORM_APP)
    return false;

  // If the old permission set can access any host, then it can't be elevated.
  if (old_permissions->HasEffectiveAccessToAllHosts())
    return false;

  // Likewise, if the new permission set has full host access, then it must be
  // a privilege increase.
  if (new_permissions->HasEffectiveAccessToAllHosts())
    return true;

  const URLPatternSet& old_list = old_permissions->effective_hosts();
  const URLPatternSet& new_list = new_permissions->effective_hosts();

  // TODO(jstritar): This is overly conservative with respect to subdomains.
  // For example, going from *.google.com to www.google.com will be
  // considered an elevation, even though it is not (http://crbug.com/65337).
  std::set<std::string> new_hosts_set(
      permission_message_util::GetDistinctHosts(new_list, false, false));
  std::set<std::string> old_hosts_set(
      permission_message_util::GetDistinctHosts(old_list, false, false));
  std::set<std::string> new_hosts_only =
      base::STLSetDifference<std::set<std::string> >(new_hosts_set,
                                                     old_hosts_set);

  return !new_hosts_only.empty();
}

}  // namespace extensions
