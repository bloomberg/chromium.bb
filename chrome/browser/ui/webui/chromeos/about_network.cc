// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/about_network.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chromeos/network/favorite_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

// Html output helper functions

std::string WrapWithH3(const std::string& text) {
  return "<h3>" + net::EscapeForHTML(text) + "</h3>";
}

std::string WrapWithTH(const std::string& text) {
  return "<th>" + net::EscapeForHTML(text) + "</th>";
}

std::string WrapWithTD(const std::string& text) {
  return "<td>" + net::EscapeForHTML(text) + "</td>";
}

std::string WrapWithTR(const std::string& text) {
  return "<tr>" + text + "</tr>";
}

void AppendRefresh(std::string *output, int refresh, const std::string& name) {
  if (refresh > 0) {
    output->append(l10n_util::GetStringFUTF8(
        IDS_ABOUT_AUTO_REFRESH, base::IntToString16(refresh)));
  } else {
    output->append(l10n_util::GetStringFUTF8(
        IDS_ABOUT_AUTO_REFRESH_INFO, UTF8ToUTF16(name)));
  }
}

std::string GetHeaderHtmlInfo(int refresh) {
  std::string output;
  ::about_ui::AppendHeader(&output, refresh, "About Network");
  ::about_ui::AppendBody(&output);
  AppendRefresh(&output, refresh, "network");
  return output;
}

std::string GetEventLogSection(bool debug) {
  std::string output;
  output.append(WrapWithH3(
      l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_EVENT_LOG)));
  output.append("<pre style=\""
                "border-style:solid; border-width:1px; "
                "overflow: auto; "
                "height:200px;\">");
  network_event_log::LogLevel log_level = debug
      ? network_event_log::LOG_LEVEL_DEBUG
      : network_event_log::LOG_LEVEL_EVENT;
  std::string format = debug ? "file,time,desc,html" : "time,desc,html";
  // network_event_log::GetAsString does HTML escaping.
  output.append(
      network_event_log::GetAsString(network_event_log::NEWEST_FIRST,
                                     format, log_level, 0));
  output.append("</pre>");
  return output;
}

std::string NetworkStateToHtmlTableHeader() {
  std::string str =
      WrapWithTH("Name") +
      WrapWithTH("Type") +
      WrapWithTH("State") +
      WrapWithTH("Path") +
      WrapWithTH("Profile") +
      WrapWithTH("Connect") +
      WrapWithTH("Error") +
      WrapWithTH("IP Addr") +
      WrapWithTH("Security") +
      WrapWithTH("Technology") +
      WrapWithTH("Activation") +
      WrapWithTH("Romaing") +
      WrapWithTH("OOC") +
      WrapWithTH("Strength") +
      WrapWithTH("Auto") +
      WrapWithTH("Fav") +
      WrapWithTH("Pri");
  return WrapWithTR(str);
}

std::string NetworkStateToHtmlTableRow(const NetworkState* network) {
  std::string str =
      WrapWithTD(network->name()) +
      WrapWithTD(network->type()) +
      WrapWithTD(network->connection_state()) +
      WrapWithTD(network->path()) +
      WrapWithTD(network->profile_path()) +
      WrapWithTD(base::IntToString(network->connectable())) +
      WrapWithTD(network->error()) +
      WrapWithTD(network->ip_address()) +
      WrapWithTD(network->security()) +
      WrapWithTD(network->network_technology()) +
      WrapWithTD(network->activation_state()) +
      WrapWithTD(network->roaming()) +
      WrapWithTD(base::IntToString(network->cellular_out_of_credits())) +
      WrapWithTD(base::IntToString(network->signal_strength())) +
      WrapWithTD(base::IntToString(network->auto_connect())) +
      WrapWithTD(base::IntToString(network->favorite())) +
      WrapWithTD(base::IntToString(network->priority()));
  return WrapWithTR(str);
}

std::string FavoriteStateToHtmlTableHeader() {
  std::string str =
      WrapWithTH("Name") +
      WrapWithTH("Type") +
      WrapWithTH("Path") +
      WrapWithTH("Profile");
  return WrapWithTR(str);
}

std::string FavoriteStateToHtmlTableRow(const FavoriteState* favorite) {
  std::string str =
      WrapWithTD(favorite->name()) +
      WrapWithTD(favorite->type()) +
      WrapWithTD(favorite->path()) +
      WrapWithTD(favorite->profile_path());
  return WrapWithTR(str);
}

std::string GetNetworkStateHtmlInfo() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  NetworkStateHandler::NetworkStateList network_list;
  handler->GetNetworkList(&network_list);

  std::string output;
  output.append(WrapWithH3(
      l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_NETWORKS)));
  output.append("<table border=1>");
  output.append(NetworkStateToHtmlTableHeader());
  for (NetworkStateHandler::NetworkStateList::iterator iter =
           network_list.begin(); iter != network_list.end(); ++iter) {
    const NetworkState* network = *iter;
    output.append(NetworkStateToHtmlTableRow(network));
  }
  output.append("</table>");
  return output;
}

std::string GetFavoriteStateHtmlInfo() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  NetworkStateHandler::FavoriteStateList favorite_list;
  handler->GetFavoriteList(&favorite_list);

  std::string output;
  output.append(WrapWithH3(
      l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_FAVORITES)));
  output.append("<table border=1>");
  output.append(FavoriteStateToHtmlTableHeader());
  for (NetworkStateHandler::FavoriteStateList::iterator iter =
           favorite_list.begin(); iter != favorite_list.end(); ++iter) {
    const FavoriteState* favorite = *iter;
    output.append(FavoriteStateToHtmlTableRow(favorite));
  }
  output.append("</table>");
  return output;
}

}  // namespace

namespace about_ui {

std::string AboutNetwork(const std::string& query) {
  base::StringTokenizer tok(query, "/");
  int refresh = 0;
  bool debug = false;
  while (tok.GetNext()) {
    std::string token = tok.token();
    if (token == "debug")
      debug = true;
    else
      base::StringToInt(token, &refresh);
  }
  std::string output = GetHeaderHtmlInfo(refresh);
  if (network_event_log::IsInitialized())
    output += GetEventLogSection(debug);
  output += GetNetworkStateHtmlInfo();
  output += GetFavoriteStateHtmlInfo();
  return output;
}

}  // namespace about_ui

}  // namespace chromeos
