// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/navigation_message_processor.h"

#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/navigation.pb.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace blimp {

NavigationMessageProcessor::NavigationMessageProcessor(
    BlimpMessageProcessor* outgoing_message_processor)
    : outgoing_message_processor_(outgoing_message_processor) {
}

NavigationMessageProcessor::~NavigationMessageProcessor() {
}

void NavigationMessageProcessor::SetDelegate(
    int tab_id,
    NavigationMessageDelegate* delegate) {
  DCHECK(!FindDelegate(tab_id));
  delegates_[tab_id] = delegate;
}

void NavigationMessageProcessor::RemoveDelegate(int tab_id) {
  DelegateMap::iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    delegates_.erase(it);
}

void NavigationMessageProcessor::NavigateToUrlText(
    int tab_id,
    const std::string& url_text) {
  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  NavigationMessage* navigation_message = blimp_message->mutable_navigation();
  navigation_message->set_type(NavigationMessage::LOAD_URL);
  navigation_message->mutable_load_url()->set_url(url_text);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationMessageProcessor::Reload(int tab_id) {
  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  NavigationMessage* navigation_message = blimp_message->mutable_navigation();
  navigation_message->set_type(NavigationMessage::RELOAD);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationMessageProcessor::GoForward(int tab_id) {
  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  NavigationMessage* navigation_message = blimp_message->mutable_navigation();
  navigation_message->set_type(NavigationMessage::GO_FORWARD);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationMessageProcessor::GoBack(int tab_id) {
  scoped_ptr<BlimpMessage> blimp_message(new BlimpMessage);
  blimp_message->set_target_tab_id(tab_id);

  NavigationMessage* navigation_message = blimp_message->mutable_navigation();
  navigation_message->set_type(NavigationMessage::GO_BACK);

  outgoing_message_processor_->ProcessMessage(std::move(blimp_message),
                                              net::CompletionCallback());
}

void NavigationMessageProcessor::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  DCHECK(message->type() == BlimpMessage::NAVIGATION);

  int tab_id = message->target_tab_id();
  DCHECK(message->has_navigation());
  const NavigationMessage& navigation_message = message->navigation();

  NavigationMessageDelegate* delegate = FindDelegate(tab_id);
  DCHECK(delegate);
  if (!delegate)
    return;

  switch (navigation_message.type()) {
    case NavigationMessage::NAVIGATION_STATE_CHANGED:
      {
        const NavigationStateChangeMessage& details =
            navigation_message.navigation_state_change();
        if (details.has_url())
          delegate->OnUrlChanged(tab_id, GURL(details.url()));

        if (details.has_title())
          delegate->OnTitleChanged(tab_id, details.title());

        if (details.has_loading())
          delegate->OnLoadingChanged(tab_id, details.loading());

        if (details.has_favicon()) {
          NOTIMPLEMENTED();
        }
      }
      break;
    case NavigationMessage::LOAD_URL:
    case NavigationMessage::GO_BACK:
    case NavigationMessage::GO_FORWARD:
    case NavigationMessage::RELOAD:
      NOTREACHED() << "Client received unexpected navigation type.";
      break;
    case NavigationMessage::UNKNOWN:
      NOTREACHED();
  }

  if (!callback.is_null())
    callback.Run(net::OK);
}

NavigationMessageProcessor::NavigationMessageDelegate*
NavigationMessageProcessor::FindDelegate(
    const int tab_id) {
  DelegateMap::const_iterator it = delegates_.find(tab_id);
  if (it != delegates_.end())
    return it->second;
  return nullptr;
}

}  // namespace blimp
