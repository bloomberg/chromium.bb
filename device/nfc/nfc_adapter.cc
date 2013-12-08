// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_adapter.h"

#include "base/stl_util.h"
#include "device/nfc/nfc_peer.h"
#include "device/nfc/nfc_tag.h"

namespace device {

NfcAdapter::NfcAdapter() {
}

NfcAdapter::~NfcAdapter() {
  STLDeleteValues(&peers_);
  STLDeleteValues(&tags_);
}

void NfcAdapter::GetPeers(PeerList* peer_list) const {
  peer_list->clear();
  for (PeersMap::const_iterator iter = peers_.begin();
       iter != peers_.end(); ++iter) {
    peer_list->push_back(iter->second);
  }
}

void NfcAdapter::GetTags(TagList* tag_list) const {
  tag_list->clear();
  for (TagsMap::const_iterator iter = tags_.begin();
       iter != tags_.end(); ++iter) {
    tag_list->push_back(iter->second);
  }
}

NfcPeer* NfcAdapter::GetPeer(const std::string& identifier) const {
  PeersMap::const_iterator iter = peers_.find(identifier);
  if (iter != peers_.end())
    return iter->second;
  return NULL;
}

NfcTag* NfcAdapter::GetTag(const std::string& identifier) const {
  TagsMap::const_iterator iter = tags_.find(identifier);
  if (iter != tags_.end())
    return iter->second;
  return NULL;
}

}  // namespace device
