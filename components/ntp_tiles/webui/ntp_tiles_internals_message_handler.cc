// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_fixer.h"
#include "url/gurl.h"

namespace ntp_tiles {

NTPTilesInternalsMessageHandlerClient::NTPTilesInternalsMessageHandlerClient() =
    default;
NTPTilesInternalsMessageHandlerClient::
    ~NTPTilesInternalsMessageHandlerClient() = default;

NTPTilesInternalsMessageHandler::NTPTilesInternalsMessageHandler()
    : client_(nullptr), site_count_(8) {}

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
}

void NTPTilesInternalsMessageHandler::HandleRegisterForEvents(
    const base::ListValue* args) {
  if (!client_->SupportsNTPTiles()) {
    return;
  }
  DCHECK(args->empty());

  SendSourceInfo();

  most_visited_sites_ = client_->MakeMostVisitedSites();
  most_visited_sites_->SetMostVisitedURLsObserver(this, site_count_);
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

  if (client_->DoesSourceExist(ntp_tiles::NTPTileSource::POPULAR)) {
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

void NTPTilesInternalsMessageHandler::SendSourceInfo() {
  PrefService* prefs = client_->GetPrefs();
  base::DictionaryValue value;

  value.SetBoolean("topSites",
                   client_->DoesSourceExist(NTPTileSource::TOP_SITES));
  value.SetBoolean(
      "suggestionsService",
      client_->DoesSourceExist(NTPTileSource::SUGGESTIONS_SERVICE));
  value.SetBoolean("whitelist",
                   client_->DoesSourceExist(NTPTileSource::WHITELIST));

  if (client_->DoesSourceExist(NTPTileSource::POPULAR)) {
    auto popular_sites = client_->MakePopularSites();
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
