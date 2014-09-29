// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/incoming_connection_listener_posix.h"

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/tracked_objects.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"
#include "net/socket/unix_domain_server_socket_posix.h"

namespace mojo {
namespace shell {

namespace {
// TODO(cmasone): Figure out what we should be doing about "authenticating" the
// process trying to connect.
bool Yes(const net::UnixDomainServerSocket::Credentials& ignored) {
  return true;
}
}  // anonymous namespace

IncomingConnectionListenerPosix::IncomingConnectionListenerPosix(
    const base::FilePath& socket_path,
    Delegate* delegate)
    : delegate_(delegate),
      socket_path_(socket_path),
      listen_socket_(base::Bind(&Yes), false),
      incoming_socket_(net::kInvalidSocket),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

IncomingConnectionListenerPosix::~IncomingConnectionListenerPosix() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (!base::DeleteFile(socket_path_, false))
    PLOG(ERROR) << "Listening Unix domain socket can't be destroyed.";
}

void IncomingConnectionListenerPosix::StartListening() {
  DCHECK(listen_thread_checker_.CalledOnValidThread());

  int rv = net::OK;
  if (!base::DirectoryExists(socket_path_.DirName())) {
    LOG(ERROR) << "Directorty for listening socket does not exist.";
    rv = net::ERR_FILE_NOT_FOUND;
  } else if (!base::PathIsWritable(socket_path_.DirName())) {
    LOG(ERROR) << "Listening socket file path is not writable.";
    rv = net::ERR_ACCESS_DENIED;
  } else if (!base::DeleteFile(socket_path_, false)) {
    PLOG(ERROR) << "Listening socket file exists and can't be deleted";
    rv = net::ERR_FILE_EXISTS;
  } else {
    const std::string& socket_address = socket_path_.value();
    rv = listen_socket_.ListenWithAddressAndPort(socket_address, 0, 100);
  }

  // Call OnListening() before Accept(), so that the delegate is certain to
  // hear about listening before a connection might be accepted below.
  delegate_->OnListening(rv);
  if (rv == net::OK)
    Accept();
}

void IncomingConnectionListenerPosix::Accept() {
  DCHECK(listen_thread_checker_.CalledOnValidThread());
  int rv = listen_socket_.AcceptSocketDescriptor(
      &incoming_socket_,
      base::Bind(&IncomingConnectionListenerPosix::OnAccept,
                 weak_ptr_factory_.GetWeakPtr()));

  // If rv == net::ERR_IO_PENDING), listen_socket_ will call
  // OnAccept() later, when a connection attempt comes in.
  if (rv != net::ERR_IO_PENDING) {
    DVLOG_IF(1, rv == net::OK) << "Accept succeeded immediately";
    OnAccept(rv);
  }
}

void IncomingConnectionListenerPosix::OnAccept(int rv) {
  DCHECK(listen_thread_checker_.CalledOnValidThread());

  if (rv != net::OK || incoming_socket_ == net::kInvalidSocket) {
    LOG_IF(ERROR, rv != net::OK) << "Accept failed " << net::ErrorToString(rv);
    PLOG_IF(ERROR, rv == net::OK) << "Socket invalid";
  } else {
    // Passes ownership of incoming_socket_ to delegate_.
    delegate_->OnConnection(incoming_socket_);
    incoming_socket_ = net::kInvalidSocket;
  }

  // Continue waiting to accept incoming connections...
  Accept();
}

}  // namespace shell
}  // namespace mojo
