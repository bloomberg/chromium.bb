// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_

#include <map>
#include <memory>
#include <vector>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_ui.h"

class ChooserContextBase;
class HostContentSettingsMap;
class Profile;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace site_settings {

// Maps from a secondary pattern to a setting.
typedef std::map<ContentSettingsPattern, ContentSetting>
    OnePatternSettings;
// Maps from a primary pattern/source pair to a OnePatternSettings. All the
// mappings in OnePatternSettings share the given primary pattern and source.
typedef std::map<std::pair<ContentSettingsPattern, std::string>,
                 OnePatternSettings>
    AllPatternsSettings;

extern const char kSetting[];
extern const char kOrigin[];
extern const char kPolicyProviderId[];
extern const char kSource[];
extern const char kEmbeddingOrigin[];
extern const char kPreferencesSource[];

// Group types.
extern const char kGroupTypeUsb[];

// Returns whether a group name has been registered for the given type.
bool HasRegisteredGroupName(ContentSettingsType type);

// Gets a content settings type from the group name identifier.
ContentSettingsType ContentSettingsTypeFromGroupName(const std::string& name);

// Gets a string identifier for the group name.
std::string ContentSettingsTypeToGroupName(ContentSettingsType type);

// Fills in |exceptions| with Values for the given |type| from |map|.
void GetExceptionsFromHostContentSettingsMap(
    const HostContentSettingsMap* map,
    ContentSettingsType type,
    content::WebUI* web_ui,
    base::ListValue* exceptions);

// Returns exceptions constructed from the policy-set allowed URLs
// for the content settings |type| mic or camera.
void GetPolicyAllowedUrls(
    ContentSettingsType type,
    std::vector<std::unique_ptr<base::DictionaryValue>>* exceptions,
    content::WebUI* web_ui);

// This struct facilitates lookup of a chooser context factory function by name
// for a given content settings type and is declared early so that it can used
// by functions below.
struct ChooserTypeNameEntry {
  ContentSettingsType type;
  ChooserContextBase* (*get_context)(Profile*);
  const char* name;
  const char* ui_name_key;
};

ChooserContextBase* GetUsbChooserContext(Profile* profile);

struct ContentSettingsTypeNameEntry {
  ContentSettingsType type;
  const char* name;
};

const ContentSettingsTypeNameEntry kContentSettingsTypeGroupNames[] = {
  {CONTENT_SETTINGS_TYPE_COOKIES, "cookies"},
  {CONTENT_SETTINGS_TYPE_IMAGES, "images"},
  {CONTENT_SETTINGS_TYPE_JAVASCRIPT, "javascript"},
  {CONTENT_SETTINGS_TYPE_PLUGINS, "plugins"},
  {CONTENT_SETTINGS_TYPE_POPUPS, "popups"},
  {CONTENT_SETTINGS_TYPE_GEOLOCATION, "location"},
  {CONTENT_SETTINGS_TYPE_NOTIFICATIONS, "notifications"},
  {CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE, "auto-select-certificate"},
  {CONTENT_SETTINGS_TYPE_FULLSCREEN, "fullscreen"},
  {CONTENT_SETTINGS_TYPE_MOUSELOCK, "mouselock"},
  {CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS, "register-protocol-handler"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC, "media-stream-mic"},
  {CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA, "media-stream-camera"},
  {CONTENT_SETTINGS_TYPE_PPAPI_BROKER, "ppapi-broker"},
  {CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS, "multiple-automatic-downloads"},
  {CONTENT_SETTINGS_TYPE_MIDI_SYSEX, "midi-sysex"},
  {CONTENT_SETTINGS_TYPE_SSL_CERT_DECISIONS, "ssl-cert-decisions"},
#if defined(OS_CHROMEOS)
  {CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER, "protectedContent"},
#endif
  {CONTENT_SETTINGS_TYPE_KEYGEN, "keygen"},
  {CONTENT_SETTINGS_TYPE_BACKGROUND_SYNC, "background-sync"},
};

const ChooserTypeNameEntry kChooserTypeGroupNames[] = {
    {CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, &GetUsbChooserContext,
     kGroupTypeUsb, "name"},
};

const ChooserTypeNameEntry* ChooserTypeFromGroupName(const std::string& name);

// Fills in |exceptions| with Values for the given |chooser_type| from map.
void GetChooserExceptionsFromProfile(
    Profile* profile,
    bool incognito,
    const ChooserTypeNameEntry& chooser_type,
    base::ListValue* exceptions);

}  // namespace site_settings

#endif  // CHROME_BROWSER_UI_WEBUI_SITE_SETTINGS_HELPER_H_
