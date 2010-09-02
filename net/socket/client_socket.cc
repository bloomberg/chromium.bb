// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket.h"

#include "base/field_trial.h"
#include "base/histogram.h"

namespace net {

ClientSocket::UseHistory::UseHistory()
    : was_ever_connected_(false),
      was_used_to_convey_data_(false),
      omnibox_speculation_(false),
      subresource_speculation_(false) {
}

ClientSocket::UseHistory::~UseHistory() {
  EmitPreconnectionHistograms();
}

void ClientSocket::UseHistory::EmitPreconnectionHistograms() const {
  DCHECK(!subresource_speculation_ || !omnibox_speculation_);
  // 0 ==> non-speculative, never connected.
  // 1 ==> non-speculative never used (but connected).
  // 2 ==> non-speculative and used.
  // 3 ==> omnibox_speculative never connected.
  // 4 ==> omnibox_speculative never used (but connected).
  // 5 ==> omnibox_speculative and used.
  // 6 ==> subresource_speculative never connected.
  // 7 ==> subresource_speculative never used (but connected).
  // 8 ==> subresource_speculative and used.
  int result;
  if (was_used_to_convey_data_)
    result = 2;
  else if (was_ever_connected_)
    result = 1;
  else
    result = 0;  // Never used, and not really connected.

  if (omnibox_speculation_)
    result += 3;
  else if (subresource_speculation_)
    result += 6;
  UMA_HISTOGRAM_ENUMERATION("Net.PreconnectUtilization2", result, 9);

  static const bool connect_backup_jobs_fieldtrial =
      FieldTrialList::Find("ConnnectBackupJobs") &&
      !FieldTrialList::Find("ConnnectBackupJobs")->group_name().empty();
  if (connect_backup_jobs_fieldtrial) {
    UMA_HISTOGRAM_ENUMERATION(
        FieldTrial::MakeName("Net.PreconnectUtilization2",
                             "ConnnectBackupJobs"),
        result, 9);
  }
}

void ClientSocket::UseHistory::set_was_ever_connected() {
  DCHECK(!was_used_to_convey_data_);
  was_ever_connected_ = true;
}

void ClientSocket::UseHistory::set_was_used_to_convey_data() {
  DCHECK(was_ever_connected_);
  was_used_to_convey_data_ = true;
}


void ClientSocket::UseHistory::set_subresource_speculation() {
  DCHECK(was_ever_connected_);
  // TODO(jar): We should transition to marking a socket (or stream) at
  // construction time as being created for speculative reasons.  This current
  // approach of trying to track use of a socket to convey data can make
  // mistakes when other sockets (such as ones sitting in the pool for a long
  // time) are issued.  Unused sockets can be left over when a when a set of
  // connections to a host are made, and one is "unlucky" and takes so long to
  // complete a connection, that another socket is used, and recycled before a
  // second connection comes available.  Similarly, re-try connections can leave
  // an original (slow to connect socket) in the pool, and that can be issued
  // to a speculative requester. In any cases such old sockets will fail when an
  // attempt is made to used them!... and then it will look like a speculative
  // socket was discarded without any user!?!?!
  if (was_used_to_convey_data_)
    return;
  subresource_speculation_ = true;
}

void ClientSocket::UseHistory::set_omnibox_speculation() {
  DCHECK(was_ever_connected_);
  if (was_used_to_convey_data_)
    return;
  omnibox_speculation_ = true;
}

bool ClientSocket::UseHistory::was_used_to_convey_data() const {
  DCHECK(!was_used_to_convey_data_ || was_ever_connected_);
  return was_used_to_convey_data_;
}

}  // namespace net
