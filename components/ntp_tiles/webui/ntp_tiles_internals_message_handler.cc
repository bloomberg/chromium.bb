// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace ntp_tiles {

namespace {

std::string FormatJson(const base::Value& value) {
  std::string pretty_printed;
  bool ok = base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &pretty_printed);
  DCHECK(ok);
  return pretty_printed;
}

}  // namespace

NTPTilesInternalsMessageHandlerClient::NTPTilesInternalsMessageHandlerClient() =
    default;
NTPTilesInternalsMessageHandlerClient::
    ~NTPTilesInternalsMessageHandlerClient() = default;

NTPTilesInternalsMessageHandler::NTPTilesInternalsMessageHandler()
    : client_(nullptr), site_count_(8), weak_ptr_factory_(this) {}

NTPTilesInternalsMessageHandler::~NTPTilesInternalsMessageHandler() = default;

void NTPTilesInternalsMessageHandler::RegisterMessages(
    NTPTilesInternalsMessageHandlerClient* client) {
  client_ = client;

  client_->RegisterMessageCallback(
      "registerForEvents",
      base::Bind(&NTPTilesInternalsMessageHandler::HandleRegisterForEvents,
                 base::Unretained(this)));

  client_->RegisterMessageCallback(
      "update", base::Bind(&NTPTilesInternalsMessageHandler::HandleUpdate,
                           base::Unretained(this)));

  client_->RegisterMessageCallback(
      "fetchSuggestions",
      base::Bind(&NTPTilesInternalsMessageHandler::HandleFetchSuggestions,
                 base::Unretained(this)));

  client_->RegisterMessageCallback(
      "viewPopularSitesJson",
      base::Bind(&NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson,
                 base::Unretained(this)));
}

void NTPTilesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  if (!client_->SupportsNTPTiles()) {
    base::DictionaryValue disabled;
    disabled.SetBoolean("topSites", false);
    disabled.SetBoolean("suggestionsService", false);
    disabled.SetBoolean("popular", false);
    disabled.SetBoolean("whitelist", false);
    client_->CallJavascriptFunction(
        "chrome.ntp_tiles_internals.receiveSourceInfo", disabled);
    SendTiles(NTPTilesVector());
    return;
  }
  DCHECK(args->empty());

  suggestions_status_.clear();
  popular_sites_json_.clear();
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->SetMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleUpdate(
    const base::ListValue* args) {
  if (!client_->SupportsNTPTiles()) {
    return;
  }

  const base::DictionaryValue* dict = nullptr;
  DCHECK_EQ(1u, args->GetSize());
  args->GetDictionary(0, &dict);
  DCHECK(dict);

  PrefService* prefs = client_->GetPrefs();

  if (most_visited_sites_->DoesSourceExist(ntp_tiles::NTPTileSource::POPULAR)) {
    popular_sites_json_.clear();

    std::string url;
    dict->GetString("popular.overrideURL", &url);
    if (url.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideURL);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideURL,
                       url_formatter::FixupURL(url, std::string()).spec());
    }

    std::string country;
    dict->GetString("popular.overrideCountry", &country);
    if (country.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideCountry);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideCountry, country);
    }

    std::string version;
    dict->GetString("popular.overrideVersion", &version);
    if (version.empty()) {
      prefs->ClearPref(ntp_tiles::prefs::kPopularSitesOverrideVersion);
    } else {
      prefs->SetString(ntp_tiles::prefs::kPopularSitesOverrideVersion, version);
    }
  }

  // Recreate to pick up new values.
  // TODO(sfiera): refresh MostVisitedSites without re-creating it, as soon as
  // that will pick up changes to the Popular Sites overrides.
  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->SetMostVisitedURLsObserver(this, site_count_);
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleFetchSuggestions(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  if (!most_visited_sites_->DoesSourceExist(
          ntp_tiles::NTPTileSource::SUGGESTIONS_SERVICE)) {
    return;
  }

  if (most_visited_sites_->suggestions()->FetchSuggestionsData()) {
    suggestions_status_ = "fetching...";
  } else {
    suggestions_status_ = "history sync is disabled, or not yet initialized";
  }
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::HandleViewPopularSitesJson(
    const base::ListValue* args) {
  DCHECK_EQ(0u, args->GetSize());
  if (!most_visited_sites_->DoesSourceExist(
          ntp_tiles::NTPTileSource::POPULAR)) {
    return;
  }

  popular_sites_json_ = FormatJson(
      *most_visited_sites_->popular_sites()->GetCachedJson());
  SendSourceInfo();
}

void NTPTilesInternalsMessageHandler::SendSourceInfo() {
  PrefService* prefs = client_->GetPrefs();
  base::DictionaryValue value;

  value.SetBoolean("topSites", most_visited_sites_->DoesSourceExist(
                                   NTPTileSource::TOP_SITES));
  value.SetBoolean("whitelist", most_visited_sites_->DoesSourceExist(
                                    NTPTileSource::WHITELIST));

  if (most_visited_sites_->DoesSourceExist(
          NTPTileSource::SUGGESTIONS_SERVICE)) {
    value.SetString("suggestionsService.status", suggestions_status_);
  } else {
    value.SetBoolean("suggestionsService", false);
  }

  if (most_visited_sites_->DoesSourceExist(NTPTileSource::POPULAR)) {
    auto* popular_sites = most_visited_sites_->popular_sites();
    value.SetString("popular.url", popular_sites->GetURLToFetch().spec());
    value.SetString("popular.country", popular_sites->GetCountryToFetch());
    value.SetString("popular.version", popular_sites->GetVersionToFetch());

    value.SetString(
        "popular.overrideURL",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideURL));
    value.SetString(
        "popular.overrideCountry",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideCountry));
    value.SetString(
        "popular.overrideVersion",
        prefs->GetString(ntp_tiles::prefs::kPopularSitesOverrideVersion));

    value.SetString("popular.json", popular_sites_json_);
  } else {
    value.SetBoolean("popular", false);
  }

  client_->CallJavascriptFunction(
      "chrome.ntp_tiles_internals.receiveSourceInfo", value);
}

void NTPTilesInternalsMessageHandler::SendTiles(const NTPTilesVector& tiles) {
  auto sites_list = base::MakeUnique<base::ListValue>();
  for (const NTPTile& tile : tiles) {
    auto entry = base::MakeUnique<base::DictionaryValue>();
    entry->SetString("title", tile.title);
    entry->SetString("url", tile.url.spec());
    entry->SetInteger("source", static_cast<int>(tile.source));
    entry->SetString("whitelistIconPath",
                     tile.whitelist_icon_path.LossyDisplayName());
    sites_list->Append(std::move(entry));
  }

  base::DictionaryValue result;
  result.Set("sites", std::move(sites_list));
  client_->CallJavascriptFunction("chrome.ntp_tiles_internals.receiveSites",
                                  result);
}

void NTPTilesInternalsMessageHandler::OnMostVisitedURLsAvailable(
    const NTPTilesVector& tiles) {
  SendTiles(tiles);
}

void NTPTilesInternalsMessageHandler::OnIconMadeAvailable(
    const GURL& site_url) {}

}  // namespace ntp_tiles
