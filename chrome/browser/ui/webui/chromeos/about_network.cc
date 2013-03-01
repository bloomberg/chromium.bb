// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/about_network.h"

#include "ash/ash_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/ui/webui/about_ui.h"
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

std::string GetHeaderEventLogInfo() {
  std::string output;
  output.append(WrapWithH3(
      l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_EVENT_LOG)));
  output.append("<pre style=\""
                "border-style:solid; border-width:1px; "
                "overflow: auto; "
                "height:200px;\">");
  output.append(
      network_event_log::GetAsString(network_event_log::NEWEST_FIRST, 0));
  output.append("</pre>");
  return output;
}

// NetworkLibrary tables

std::string NetworkToHtmlTableHeader(const Network* network) {
  std::string str =
      WrapWithTH("Name") +
      WrapWithTH("Active") +
      WrapWithTH("State");
  if (network->type() != TYPE_ETHERNET)
    str += WrapWithTH("Auto-Connect");
  if (network->type() == TYPE_WIFI ||
      network->type() == TYPE_CELLULAR) {
    str += WrapWithTH("Strength");
  }
  if (network->type() == TYPE_WIFI) {
    str += WrapWithTH("Encryption");
    str += WrapWithTH("Passphrase");
    str += WrapWithTH("Identity");
    str += WrapWithTH("Frequency");
  }
  if (network->type() == TYPE_CELLULAR) {
    str += WrapWithTH("Technology");
    str += WrapWithTH("Connectivity");
    str += WrapWithTH("Activation");
    str += WrapWithTH("Roaming");
  }
  if (network->type() == TYPE_VPN) {
    str += WrapWithTH("Host");
    str += WrapWithTH("Provider Type");
    str += WrapWithTH("PSK Passphrase");
    str += WrapWithTH("Username");
    str += WrapWithTH("User Passphrase");
  }
  str += WrapWithTH("Error");
  str += WrapWithTH("IP Address");
  return WrapWithTR(str);
}

// Helper function to create an Html table row for a Network.
std::string NetworkToHtmlTableRow(const Network* network) {
  std::string str =
      WrapWithTD(network->name()) +
      WrapWithTD(base::IntToString(network->is_active())) +
      WrapWithTD(network->GetStateString());
  if (network->type() != TYPE_ETHERNET)
    str += WrapWithTD(base::IntToString(network->auto_connect()));
  if (network->type() == TYPE_WIFI ||
      network->type() == TYPE_CELLULAR) {
    const WirelessNetwork* wireless =
        static_cast<const WirelessNetwork*>(network);
    str += WrapWithTD(base::IntToString(wireless->strength()));
  }
  if (network->type() == TYPE_WIFI) {
    const WifiNetwork* wifi =
        static_cast<const WifiNetwork*>(network);
    str += WrapWithTD(wifi->GetEncryptionString());
    str += WrapWithTD(std::string(wifi->passphrase().length(), '*'));
    str += WrapWithTD(wifi->identity());
    str += WrapWithTD(base::IntToString(wifi->frequency()));
  }
  if (network->type() == TYPE_CELLULAR) {
    const CellularNetwork* cell =
        static_cast<const CellularNetwork*>(network);
    str += WrapWithTH(cell->GetNetworkTechnologyString());
    str += WrapWithTH(cell->GetActivationStateString());
    str += WrapWithTH(cell->GetRoamingStateString());
  }
  if (network->type() == TYPE_VPN) {
    const VirtualNetwork* vpn =
        static_cast<const VirtualNetwork*>(network);
    str += WrapWithTH(vpn->server_hostname());
    str += WrapWithTH(vpn->GetProviderTypeString());
    str += WrapWithTD(std::string(vpn->psk_passphrase().length(), '*'));
    str += WrapWithTH(vpn->username());
    str += WrapWithTD(std::string(vpn->user_passphrase().length(), '*'));
  }
  str += WrapWithTD(network->failed() ? network->GetErrorString() : "");
  str += WrapWithTD(network->ip_address());
  return WrapWithTR(str);
}

std::string GetCrosNetworkHtmlInfo() {
  std::string output;

  NetworkLibrary* cros = CrosLibrary::Get()->GetNetworkLibrary();

  const EthernetNetwork* ethernet = cros->ethernet_network();
  if (cros->ethernet_enabled() && ethernet) {
    output.append(WrapWithH3(
        l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_ETHERNET)));
    output.append("<table border=1>");
    output.append(NetworkToHtmlTableHeader(ethernet));
    output.append(NetworkToHtmlTableRow(ethernet));
    output.append("</table>");
  }

  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  if (cros->wifi_enabled() && wifi_networks.size() > 0) {
    output.append(WrapWithH3(
        l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_WIFI)));
    output.append("<table border=1>");
    for (size_t i = 0; i < wifi_networks.size(); ++i) {
      if (i == 0)
        output.append(NetworkToHtmlTableHeader(wifi_networks[i]));
      output.append(NetworkToHtmlTableRow(wifi_networks[i]));
    }
    output.append("</table>");
  }

  const CellularNetworkVector& cellular_networks = cros->cellular_networks();
  if (cros->cellular_enabled() && cellular_networks.size() > 0) {
    output.append(WrapWithH3(
        l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_CELLULAR)));
    output.append("<table border=1>");
    for (size_t i = 0; i < cellular_networks.size(); ++i) {
      if (i == 0)
        output.append(NetworkToHtmlTableHeader(cellular_networks[i]));
      output.append(NetworkToHtmlTableRow(cellular_networks[i]));
    }
    output.append("</table>");
  }

  const VirtualNetworkVector& virtual_networks = cros->virtual_networks();
  if (virtual_networks.size() > 0) {
    output.append(WrapWithH3(
        l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_VIRTUAL)));
    output.append("<table border=1>");
    for (size_t i = 0; i < virtual_networks.size(); ++i) {
      if (i == 0)
        output.append(NetworkToHtmlTableHeader(virtual_networks[i]));
      output.append(NetworkToHtmlTableRow(virtual_networks[i]));
    }
    output.append("</table>");
  }

  const WifiNetworkVector& remembered_wifi_networks =
      cros->remembered_wifi_networks();
  if (remembered_wifi_networks.size() > 0) {
    output.append(WrapWithH3(
        l10n_util::GetStringUTF8(IDS_ABOUT_NETWORK_REMEMBERED_WIFI)));
    output.append("<table border=1>");
    for (size_t i = 0; i < remembered_wifi_networks.size(); ++i) {
      if (i == 0)
        output.append(NetworkToHtmlTableHeader(remembered_wifi_networks[i]));
      output.append(NetworkToHtmlTableRow(remembered_wifi_networks[i]));
    }
    output.append("</table>");
  }

  ::about_ui::AppendFooter(&output);
  return output;
}

// NetworkStateHandler tables

std::string NetworkStateToHtmlTableHeader() {
  std::string str =
      WrapWithTH("Name") +
      WrapWithTH("Type") +
      WrapWithTH("State") +
      WrapWithTH("Path") +
      WrapWithTH("IP Addr") +
      WrapWithTH("Security") +
      WrapWithTH("Technology") +
      WrapWithTH("Activation") +
      WrapWithTH("Romaing") +
      WrapWithTH("Strength");
  return WrapWithTR(str);
}

std::string NetworkStateToHtmlTableRow(const NetworkState* network) {
  std::string str =
      WrapWithTD(network->name()) +
      WrapWithTD(network->type()) +
      WrapWithTD(network->connection_state()) +
      WrapWithTD(network->path()) +
      WrapWithTD(network->ip_address()) +
      WrapWithTD(network->security()) +
      WrapWithTD(network->technology()) +
      WrapWithTD(network->activation_state()) +
      WrapWithTD(network->roaming()) +
      WrapWithTD(base::IntToString(network->signal_strength()));
  return WrapWithTR(str);
}

std::string GetNetworkStateHtmlInfo() {
  NetworkStateHandler* handler = NetworkStateHandler::Get();
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

}  // namespace

namespace about_ui {

std::string AboutNetwork(const std::string& query) {
  int refresh;
  base::StringToInt(query, &refresh);
  std::string output = GetHeaderHtmlInfo(refresh);
  if (network_event_log::IsInitialized())
    output += GetHeaderEventLogInfo();
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kAshEnableNewNetworkStatusArea)) {
    output += GetNetworkStateHtmlInfo();
  } else {
    output += GetCrosNetworkHtmlInfo();
  }
  return output;
}

}  // namespace about_ui

}  // namespace chromeos
