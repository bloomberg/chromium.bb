// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H_
#pragma once

#include <string>

class DictionaryValue;
class GURL;
class PrefService;

class ExternalProtocolHandler {
 public:
  enum BlockState {
    DONT_BLOCK,
    BLOCK,
    UNKNOWN,
  };

  // Returns whether we should block a given scheme.
  static BlockState GetBlockState(const std::string& scheme);

  // Sets whether we should block a given scheme.
  static void SetBlockState(const std::string& scheme, BlockState state);

  // Checks to see if the protocol is allowed, if it is whitelisted,
  // the application associated with the protocol is launched on the io thread,
  // if it is blacklisted, returns silently. Otherwise, an
  // ExternalProtocolDialog is created asking the user. If the user accepts,
  // LaunchUrlWithoutSecurityCheck is called on the io thread and the
  // application is launched.
  // Must run on the UI thread.
  static void LaunchUrl(const GURL& url, int render_process_host_id,
                        int tab_contents_id);

  // Creates and runs a External Protocol dialog box.
  // |url| - The url of the request.
  // |render_process_host_id| and |routing_id| are used by
  // tab_util::GetTabContentsByID to aquire the tab contents associated with
  // this dialog.
  // NOTE: There is a race between the Time of Check and the Time Of Use for
  //       the command line. Since the caller (web page) does not have access
  //       to change the command line by itself, we do not do anything special
  //       to protect against this scenario.
  // This is implemented separately on each platform.
  static void RunExternalProtocolDialog(const GURL& url,
                                        int render_process_host_id,
                                        int routing_id);

  // Register the ExcludedSchemes preference.
  static void RegisterPrefs(PrefService* prefs);

  // Starts a url using the external protocol handler with the help
  // of shellexecute. Should only be called if the protocol is whitelisted
  // (checked in LaunchUrl) or if the user explicitly allows it. (By selecting
  // "Launch Application" in an ExternalProtocolDialog.) It is assumed that the
  // url has already been escaped, which happens in LaunchUrl.
  // NOTE: You should Not call this function directly unless you are sure the
  // url you have has been checked against the blacklist, and has been escaped.
  // All calls to this function should originate in some way from LaunchUrl.
  // This will execute on the file thread.
  static void LaunchUrlWithoutSecurityCheck(const GURL& url);

  // Prepopulates the dictionary with known protocols to deny or allow, if
  // preferences for them do not already exist.
  static void PrepopulateDictionary(DictionaryValue* win_pref);

  // Allows LaunchUrl to proceed with launching an external protocol handler.
  // This is typically triggered by a user gesture, but is also called for
  // each extension API function. Note that each call to LaunchUrl resets
  // the state to false (not allowed).
  static void PermitLaunchUrl();
};

#endif  // CHROME_BROWSER_EXTERNAL_PROTOCOL_HANDLER_H_
