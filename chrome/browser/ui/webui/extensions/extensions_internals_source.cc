// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_internals_source.h"

#include <string>
#include <unordered_map>
#include <utility>

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/activity.h"
#include "extensions/browser/event_listener_map.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_manager.h"

namespace {

const char* TypeToString(extensions::Manifest::Type type) {
  switch (type) {
    case extensions::Manifest::TYPE_UNKNOWN:
      return "TYPE_UNKNOWN";
    case extensions::Manifest::TYPE_EXTENSION:
      return "TYPE_EXTENSION";
    case extensions::Manifest::TYPE_THEME:
      return "TYPE_THEME";
    case extensions::Manifest::TYPE_USER_SCRIPT:
      return "TYPE_USER_SCRIPT";
    case extensions::Manifest::TYPE_HOSTED_APP:
      return "TYPE_HOSTED_APP";
    case extensions::Manifest::TYPE_LEGACY_PACKAGED_APP:
      return "TYPE_LEGACY_PACKAGED_APP";
    case extensions::Manifest::TYPE_PLATFORM_APP:
      return "TYPE_PLATFORM_APP";
    case extensions::Manifest::TYPE_SHARED_MODULE:
      return "TYPE_SHARED_MODULE";
    case extensions::Manifest::NUM_LOAD_TYPES:
      break;
  }
  NOTREACHED();
  return "";
}

const char* LocationToString(extensions::Manifest::Location loc) {
  switch (loc) {
    case extensions::Manifest::INVALID_LOCATION:
      return "INVALID_LOCATION";
    case extensions::Manifest::INTERNAL:
      return "INTERNAL";
    case extensions::Manifest::EXTERNAL_PREF:
      return "EXTERNAL_PREF";
    case extensions::Manifest::EXTERNAL_REGISTRY:
      return "EXTERNAL_REGISTRY";
    case extensions::Manifest::UNPACKED:
      return "UNPACKED";
    case extensions::Manifest::COMPONENT:
      return "COMPONENT";
    case extensions::Manifest::EXTERNAL_PREF_DOWNLOAD:
      return "EXTERNAL_PREF_DOWNLOAD";
    case extensions::Manifest::EXTERNAL_POLICY_DOWNLOAD:
      return "EXTERNAL_POLICY_DOWNLOAD";
    case extensions::Manifest::COMMAND_LINE:
      return "COMMAND_LINE";
    case extensions::Manifest::EXTERNAL_POLICY:
      return "EXTERNAL_POLICY";
    case extensions::Manifest::EXTERNAL_COMPONENT:
      return "EXTERNAL_COMPONENT";
    case extensions::Manifest::NUM_LOCATIONS:
      break;
  }
  NOTREACHED();
  return "";
}

// The JSON we generate looks like this:
//
// [ {
//    "event_listeners": {
//       "count": 2,
//       "events": [ {
//          "name": "runtime.onInstalled"
//       }, {
//          "name": "runtime.onSuspend"
//       } ]
//    },
//    "id": "bhloflhklmhfpedakmangadcdofhnnoh",
//    "keepalive": {
//       "activities": [ {
//          "extra_data": "render-frame",
//          "type": "PROCESS_MANAGER"
//       } ],
//       "count": 1
//    },
//    "location": "INTERNAL",
//    "manifest_version": 2,
//    "name": "Earth View from Google Earth",
//    "path": "/user/Extensions/bhloflhklmhfpedakmangadcdofhnnoh/2.18.5_0",
//    "type": "TYPE_EXTENSION",
//    "version": "2.18.5"
// } ]
//
// Which is:
//
// LIST
//  DICT
//    "event_listeners": DICT
//      "count": INT
//      "events": LIST
//        DICT
//          "name": STRING
//          "filter": DICT
//    "id": STRING
//    "keepalive": DICT
//      "activities": LIST
//        DICT
//          "extra_data": STRING
//          "type": STRING
//      "count": INT
//    "location": STRING
//    "manifest_version": INT
//    "name": STRING
//    "path": STRING
//    "type": STRING
//    "version": STRING

constexpr base::StringPiece kActivitesKey = "activites";
constexpr base::StringPiece kCountKey = "count";
constexpr base::StringPiece kEventsKey = "events";
constexpr base::StringPiece kEventsListenersKey = "event_listeners";
constexpr base::StringPiece kExtraDataKey = "extra_data";
constexpr base::StringPiece kFilterKey = "filter";
constexpr base::StringPiece kInternalsIdKey = "id";
constexpr base::StringPiece kInternalsNameKey = "name";
constexpr base::StringPiece kInternalsVersionKey = "version";
constexpr base::StringPiece kKeepaliveKey = "keepalive";
constexpr base::StringPiece kLocationKey = "location";
constexpr base::StringPiece kManifestVersionKey = "manifest_version";
constexpr base::StringPiece kPathKey = "path";
constexpr base::StringPiece kTypeKey = "type";

base::Value FormatKeepaliveData(extensions::ProcessManager* process_manager,
                                const extensions::Extension* extension) {
  base::Value keepalive_data(base::Value::Type::DICTIONARY);
  keepalive_data.SetKey(
      kCountKey,
      base::Value(process_manager->GetLazyKeepaliveCount(extension)));
  const extensions::ProcessManager::ActivitiesMultiset activities =
      process_manager->GetLazyKeepaliveActivities(extension);
  base::Value activities_data(base::Value::Type::LIST);
  activities_data.GetList().reserve(activities.size());
  for (const auto& activity : activities) {
    base::Value activities_entry(base::Value::Type::DICTIONARY);
    activities_entry.SetKey(
        kTypeKey, base::Value(extensions::Activity::ToString(activity.first)));
    activities_entry.SetKey(kExtraDataKey, base::Value(activity.second));
    activities_data.GetList().push_back(std::move(activities_entry));
  }
  keepalive_data.SetKey(kActivitesKey, std::move(activities_data));
  return keepalive_data;
}

void AddEventListenerData(extensions::EventRouter* event_router,
                          base::Value* data) {
  CHECK(data->is_list());
  // A map of extension ID to the event data for that extension,
  // which is of type LIST of DICTIONARY.
  std::unordered_map<base::StringPiece, base::Value, base::StringPieceHash>
      events_map;

  // Build the map of extension IDs to the list of events.
  for (const auto& entry : event_router->listeners().listeners()) {
    for (const auto& listener_entry : entry.second) {
      auto& events_list = events_map[listener_entry->extension_id()];
      if (events_list.is_none()) {
        // Not there, so make it a LIST.
        events_list = base::Value(base::Value::Type::LIST);
      }
      // The data for each event is a dictionary, with a name and a
      // filter.
      base::Value event_data(base::Value::Type::DICTIONARY);
      event_data.SetKey(kInternalsNameKey,
                        base::Value(listener_entry->event_name()));
      // Add the filter if one exists.
      base::Value* const filter = listener_entry->filter();
      if (filter != nullptr) {
        event_data.SetKey(kFilterKey, filter->Clone());
      }
      events_list.GetList().push_back(std::move(event_data));
    }
  }

  // Move all of the entries from the map into the output data.
  for (auto& output_entry : data->GetList()) {
    const base::Value* const value = output_entry.FindKey(kInternalsIdKey);
    CHECK(value && value->is_string());
    const auto it = events_map.find(value->GetString());
    base::Value listeners(base::Value::Type::DICTIONARY);
    if (it == events_map.end()) {
      // We didn't find any events, so initialize an empty dictionary.
      listeners.SetKey(kCountKey, base::Value(0));
      listeners.SetKey(kEventsKey, base::Value(base::Value::Type::LIST));
    } else {
      // Set the count and the events values.
      listeners.SetKey(
          kCountKey,
          base::Value(base::checked_cast<int>(it->second.GetList().size())));
      listeners.SetKey(kEventsKey, std::move(it->second));
    }
    output_entry.SetKey(kEventsListenersKey, std::move(listeners));
  }
}

}  // namespace

ExtensionsInternalsSource::ExtensionsInternalsSource(Profile* profile)
    : profile_(profile) {}

ExtensionsInternalsSource::~ExtensionsInternalsSource() = default;

std::string ExtensionsInternalsSource::GetSource() const {
  return chrome::kChromeUIExtensionsInternalsHost;
}

std::string ExtensionsInternalsSource::GetMimeType(
    const std::string& path) const {
  return "text/plain";
}

void ExtensionsInternalsSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  std::string json = WriteToString();
  callback.Run(base::RefCountedString::TakeString(&json));
}

std::string ExtensionsInternalsSource::WriteToString() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<extensions::ExtensionSet> extensions =
      extensions::ExtensionRegistry::Get(profile_)
          ->GenerateInstalledExtensionsSet();
  extensions::ProcessManager* process_manager =
      extensions::ProcessManager::Get(profile_);
  base::Value data(base::Value::Type::LIST);
  for (const auto& extension : *extensions) {
    base::Value extension_data(base::Value::Type::DICTIONARY);
    extension_data.SetKey(kInternalsIdKey, base::Value(extension->id()));
    extension_data.SetKey(
        kKeepaliveKey, FormatKeepaliveData(process_manager, extension.get()));
    extension_data.SetKey(kLocationKey,
                          base::Value(LocationToString(extension->location())));
    extension_data.SetKey(kManifestVersionKey,
                          base::Value(extension->manifest_version()));
    extension_data.SetKey(kInternalsNameKey, base::Value(extension->name()));
    extension_data.SetKey(kPathKey,
                          base::Value(extension->path().LossyDisplayName()));
    extension_data.SetKey(kTypeKey,
                          base::Value(TypeToString(extension->GetType())));
    extension_data.SetKey(kInternalsVersionKey,
                          base::Value(extension->GetVersionForDisplay()));
    data.GetList().push_back(std::move(extension_data));
  }

  // Aggregate and add the data for the registered event listeners.
  AddEventListenerData(extensions::EventRouter::Get(profile_), &data);

  std::string json;
  base::JSONWriter::WriteWithOptions(
      data, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

  return json;
}
