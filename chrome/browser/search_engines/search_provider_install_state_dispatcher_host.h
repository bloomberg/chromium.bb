// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_DISPATCHER_HOST_H_
#define CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_DISPATCHER_HOST_H_

#include "base/basictypes.h"

namespace IPC {
class Message;
}

class GURL;
class Profile;
class ResourceMessageFilter;
struct ViewHostMsg_GetSearchProviderInstallState_Params;

// Handles messages regarding search provider install state on the I/O thread.
class SearchProviderInstallStateDispatcherHost {
 public:
  // Unlike the other methods, the constructor is called on the UI thread.
  SearchProviderInstallStateDispatcherHost(ResourceMessageFilter* ipc_sender,
                                           Profile* profile);
  ~SearchProviderInstallStateDispatcherHost();

  // Send a message to the renderer process.
  void Send(IPC::Message* message);

  // Called to possibly handle the incoming IPC message. Returns true if
  // handled.
  bool OnMessageReceived(const IPC::Message& message, bool* message_was_ok);

 private:
  // Figures out the install state for the search provider.
  ViewHostMsg_GetSearchProviderInstallState_Params
      GetSearchProviderInstallState(const GURL& page_location,
                                    const GURL& requested_host);

  // Starts handling the message requesting the search provider install state.
  void OnMsgGetSearchProviderInstallState(const GURL& page_location,
                                          const GURL& requested_host,
                                          IPC::Message* reply_msg);

  // Sends the reply message about the search provider install state.
  void ReplyWithProviderInstallState(
      IPC::Message* reply_msg,
      ViewHostMsg_GetSearchProviderInstallState_Params install_state);

  // Used to reply to messages.
  ResourceMessageFilter* ipc_sender_;

  // Copied from the profile since the profile can't be accessed on the I/O
  // thread.
  const bool is_off_the_record_;

  DISALLOW_COPY_AND_ASSIGN(SearchProviderInstallStateDispatcherHost);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_SEARCH_PROVIDER_INSTALL_STATE_DISPATCHER_HOST_H_
