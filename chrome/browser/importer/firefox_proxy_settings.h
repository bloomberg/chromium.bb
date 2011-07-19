// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_FIREFOX_PROXY_SETTINGS_H_
#define CHROME_BROWSER_IMPORTER_FIREFOX_PROXY_SETTINGS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"

class FilePath;

namespace net {
class ProxyConfig;
}

class FirefoxProxySettings {
 public:
  enum ProxyConfig {
    NO_PROXY = 0,    // No proxy are used.
    AUTO_DETECT,     // Automatically detected.
    SYSTEM,          // Using system proxy settings.
    AUTO_FROM_URL,   // Automatically configured from a URL.
    MANUAL           // User specified settings.
  };

  enum SOCKSVersion {
    UNKNONW = 0,
    V4,
    V5
  };

  FirefoxProxySettings();
  ~FirefoxProxySettings();

  // Sets |settings| to the proxy settings for the current installed version of
  // Firefox and returns true if successful.
  // Returns false if Firefox is not installed or if the settings could not be
  // retrieved.
  static bool GetSettings(FirefoxProxySettings* settings);

  // Resets all the states of this FirefoxProxySettings to no proxy.
  void Reset();

  ProxyConfig config_type() const { return config_type_; }

  std::string http_proxy() const { return http_proxy_; }
  int http_proxy_port() const { return http_proxy_port_; }

  std::string ssl_proxy() const { return ssl_proxy_; }
  int ssl_proxy_port() const { return ssl_proxy_port_; }

  std::string ftp_proxy() const { return ftp_proxy_; }
  int ftp_proxy_port() const { return ftp_proxy_port_; }

  std::string gopher_proxy() const { return gopher_proxy_; }
  int gopher_proxy_port() const { return gopher_proxy_port_; }

  std::string socks_host() const { return socks_host_; }
  int socks_port() const { return socks_port_; }
  SOCKSVersion socks_version() const { return socks_version_; }

  std::vector<std::string> proxy_bypass_list() const {
    return proxy_bypass_list_;
  }

  const std::string autoconfig_url() const {
    return autoconfig_url_;
  }

  // Converts a FirefoxProxySettings object to a net::ProxyConfig.
  // On success returns true and fills |config| with the result.
  bool ToProxyConfig(net::ProxyConfig* config);

 protected:
  // Gets the settings from the passed prefs.js file and returns true if
  // successful.
  // Protected for tests.
  static bool GetSettingsFromFile(const FilePath& pref_file,
                                  FirefoxProxySettings* settings);

 private:
  ProxyConfig config_type_;

  std::string http_proxy_;
  int http_proxy_port_;

  std::string ssl_proxy_;
  int ssl_proxy_port_;

  std::string ftp_proxy_;
  int ftp_proxy_port_;

  std::string gopher_proxy_;
  int gopher_proxy_port_;

  std::string socks_host_;
  int socks_port_;
  SOCKSVersion socks_version_;

  std::vector<std::string> proxy_bypass_list_;

  std::string autoconfig_url_;

  DISALLOW_COPY_AND_ASSIGN(FirefoxProxySettings);
};

#endif  // CHROME_BROWSER_IMPORTER_FIREFOX_PROXY_SETTINGS_H_
