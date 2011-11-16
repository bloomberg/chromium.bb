// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_MESSAGE_FILTER_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_MESSAGE_FILTER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/search_engines/search_provider_install_data.h"
#include "chrome/common/search_provider.h"
#include "content/browser/browser_message_filter.h"

class GURL;
class Profile;

// Handles messages regarding search provider install state on the I/O thread.
class SearchProviderInstallStateMessageFilter : public BrowserMessageFilter {
 public:
  // Unlike the other methods, the constructor is called on the UI thread.
  SearchProviderInstallStateMessageFilter(int render_process_id,
                                          Profile* profile);
  virtual ~SearchProviderInstallStateMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

 private:
  // Figures out the install state for the search provider.
   search_provider::InstallState GetSearchProviderInstallState(
      const GURL& page_location,
      const GURL& requested_host);

  // Starts handling the message requesting the search provider install state.
  void OnMsgGetSearchProviderInstallState(const GURL& page_location,
                                          const GURL& requested_host,
                                          IPC::Message* reply_msg);

  // Sends the reply message about the search provider install state.
  void ReplyWithProviderInstallState(const GURL& page_location,
                                     const GURL& requested_host,
                                     IPC::Message* reply_msg);

  // Used to schedule invocations of ReplyWithProviderInstallState.
  base::WeakPtrFactory<SearchProviderInstallStateMessageFilter> weak_factory_;

  // Used to do a load and get information about install states.
  SearchProviderInstallData provider_data_;

  // Copied from the profile since the profile can't be accessed on the I/O
  // thread.
  const bool is_off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallStateMessageFilter);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_MESSAGE_FILTER_H_
