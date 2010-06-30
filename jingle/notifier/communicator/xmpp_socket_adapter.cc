// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/xmpp_socket_adapter.h"

#include <iomanip>
#include <string>

#include "base/logging.h"
#include "jingle/notifier/base/ssl_adapter.h"
#include "jingle/notifier/communicator/product_info.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/logging.h"
#include "talk/base/socketadapters.h"
#include "talk/base/ssladapter.h"
#include "talk/base/thread.h"
#include "talk/xmpp/xmppengine.h"

namespace notifier {

XmppSocketAdapter::XmppSocketAdapter(const buzz::XmppClientSettings& xcs,
                                     bool allow_unverified_certs)
  : state_(STATE_CLOSED),
    error_(ERROR_NONE),
    wsa_error_(0),
    socket_(NULL),
    protocol_(xcs.protocol()),
    firewall_(false),
    write_buffer_(NULL),
    write_buffer_length_(0),
    write_buffer_capacity_(0),
    allow_unverified_certs_(allow_unverified_certs) {
  proxy_.type = xcs.proxy();
  proxy_.address.SetIP(xcs.proxy_host());
  proxy_.address.SetPort(xcs.proxy_port());
  proxy_.username = xcs.proxy_user();
  proxy_.password = xcs.proxy_pass();
}

XmppSocketAdapter::~XmppSocketAdapter() {
  FreeState();

  // Clean up any previous socket - cannot delete socket on close because close
  // happens during the child socket's stack callback.
  if (socket_) {
    delete socket_;
    socket_ = NULL;
  }
}

bool XmppSocketAdapter::FreeState() {
  int code = 0;

  // Clean up the socket.
  if (socket_ && !(state_ == STATE_CLOSED || state_ == STATE_CLOSING)) {
    code = socket_->Close();
  }

  delete[] write_buffer_;
  write_buffer_ = NULL;
  write_buffer_length_ = 0;
  write_buffer_capacity_ = 0;

  if (code) {
    SetWSAError(code);
    return false;
  }
  return true;
}

bool XmppSocketAdapter::Connect(const talk_base::SocketAddress& addr) {
  if (state_ != STATE_CLOSED) {
    SetError(ERROR_WRONGSTATE);
    return false;
  }

  LOG(INFO) << "XmppSocketAdapter::Connect(" << addr.ToString() << ")";

  // Clean up any previous socket - cannot delete socket on close because close
  // happens during the child socket's stack callback.
  if (socket_) {
    delete socket_;
    socket_ = NULL;
  }

  talk_base::AsyncSocket* socket =
      talk_base::Thread::Current()->socketserver()->CreateAsyncSocket(
          SOCK_STREAM);
  if (!socket) {
    SetWSAError(WSA_NOT_ENOUGH_MEMORY);
    return false;
  }

  if (firewall_) {
    // TODO(sync): Change this to make WSAAsyncSockets support current thread
    // socket server.
    talk_base::FirewallSocketServer* fw =
        static_cast<talk_base::FirewallSocketServer*>(
            talk_base::Thread::Current()->socketserver());
    socket = fw->WrapSocket(socket, SOCK_STREAM);
  }

  if (proxy_.type) {
    talk_base::AsyncSocket* proxy_socket = 0;
    if (proxy_.type == talk_base::PROXY_SOCKS5) {
      proxy_socket = new talk_base::AsyncSocksProxySocket(
          socket, proxy_.address, proxy_.username, proxy_.password);
    } else {
      // Note: we are trying unknown proxies as HTTPS currently.
      proxy_socket = new talk_base::AsyncHttpsProxySocket(socket,
          GetUserAgentString(), proxy_.address, proxy_.username,
          proxy_.password);
    }
    if (!proxy_socket) {
      SetWSAError(WSA_NOT_ENOUGH_MEMORY);
      delete socket;
      return false;
    }
    socket = proxy_socket;  // For our purposes the proxy is now the socket.
  }

  if (protocol_ == cricket::PROTO_SSLTCP) {
    talk_base::AsyncSocket *fake_ssl_socket =
        new talk_base::AsyncSSLSocket(socket);
    if (!fake_ssl_socket) {
      SetWSAError(WSA_NOT_ENOUGH_MEMORY);
      delete socket;
      return false;
    }
    socket = fake_ssl_socket;  // For our purposes the SSL socket is the socket.
  }

#if defined(FEATURE_ENABLE_SSL)
  talk_base::SSLAdapter* ssl_adapter = notifier::CreateSSLAdapter(socket);
  socket = ssl_adapter;  // For our purposes the SSL adapter is the socket.
#endif

  socket->SignalReadEvent.connect(this, &XmppSocketAdapter::OnReadEvent);
  socket->SignalWriteEvent.connect(this, &XmppSocketAdapter::OnWriteEvent);
  socket->SignalConnectEvent.connect(this, &XmppSocketAdapter::OnConnectEvent);
  socket->SignalCloseEvent.connect(this, &XmppSocketAdapter::OnCloseEvent);

  // The linux implementation of socket::Connect returns an error when the
  // connect didn't complete yet.  This can be distinguished from a failure
  // because socket::IsBlocking is true.  Perhaps, the linux implementation
  // should be made to behave like the windows version which doesn't do this,
  // but it seems to be a pattern with these methods that they return an error
  // if the operation didn't complete in a sync fashion and one has to check
  // IsBlocking to tell if was a "real" error.
  if (socket->Connect(addr) == SOCKET_ERROR && !socket->IsBlocking()) {
    SetWSAError(socket->GetError());
    delete socket;
    return false;
  }

  socket_ = socket;
  state_ = STATE_CONNECTING;
  return true;
}

bool XmppSocketAdapter::Read(char* data, size_t len, size_t* len_read) {
  if (len_read)
    *len_read = 0;

  if (state_ <= STATE_CLOSING) {
    SetError(ERROR_WRONGSTATE);
    return false;
  }

  DCHECK(socket_);

  if (IsOpen()) {
    int result = socket_->Recv(data, len);
    if (result < 0) {
      if (!socket_->IsBlocking()) {
        SetWSAError(socket_->GetError());
        return false;
      }

      result = 0;
    }

    if (len_read)
      *len_read = result;
  }

  return true;
}

bool XmppSocketAdapter::Write(const char* data, size_t len) {
  if (state_ <= STATE_CLOSING) {
    // There may be data in a buffer that gets lost.  Too bad!
    SetError(ERROR_WRONGSTATE);
    return false;
  }

  DCHECK(socket_);

  size_t sent = 0;

  // Try an immediate write when there is no buffer and we aren't in SSL mode
  // or opening the connection.
  if (write_buffer_length_ == 0 && IsOpen()) {
    int result = socket_->Send(data, len);
    if (result < 0) {
      if (!socket_->IsBlocking()) {
        SetWSAError(socket_->GetError());
        return false;
      }
      result = 0;
    }

    sent = static_cast<size_t>(result);
  }

  // Buffer what we didn't send.
  if (sent < len) {
    QueueWriteData(data + sent, len - sent);
  }

  // Service the socket right away to push the written data out in SSL mode.
  return HandleWritable();
}

bool XmppSocketAdapter::Close() {
  if (state_ == STATE_CLOSING) {
    return false;  // Avoid recursion, but not unexpected.
  }
  if (state_ == STATE_CLOSED) {
    // In theory should not be trying to re-InternalClose.
    SetError(ERROR_WRONGSTATE);
    return false;
  }

  // TODO(sync): deal with flushing close (flush, don't do reads, clean ssl).

  // If we've gotten to the point where we really do have a socket underneath
  // then close it.  It should call us back to tell us it is closed, and
  // NotifyClose will be called.  We indicate "closing" state so that we
  // do not recusively try to keep closing the socket.
  if (socket_) {
    state_ = STATE_CLOSING;
    socket_->Close();
  }

  // If we didn't get the callback, then we better make sure we signal
  // closed.
  if (state_ != STATE_CLOSED) {
    // The socket was closed manually, not directly due to error.
    if (error_ != ERROR_NONE) {
      LOG(INFO) << "XmppSocketAdapter::Close - previous Error: " << error_
                << " WSAError: " << wsa_error_;
      error_ = ERROR_NONE;
      wsa_error_ = 0;
    }
    NotifyClose();
  }
  return true;
}

void XmppSocketAdapter::NotifyClose() {
  if (state_ == STATE_CLOSED) {
    SetError(ERROR_WRONGSTATE);
  } else {
    LOG(INFO) << "XmppSocketAdapter::NotifyClose - Error: " << error_
              << " WSAError: " << wsa_error_;
    state_ = STATE_CLOSED;
    SignalClosed();
    FreeState();
  }
}

void XmppSocketAdapter::OnConnectEvent(talk_base::AsyncSocket *socket) {
  if (state_ == STATE_CONNECTING) {
    state_ = STATE_OPEN;
    LOG(INFO) << "XmppSocketAdapter::OnConnectEvent - STATE_OPEN";
    SignalConnected();
#if defined(FEATURE_ENABLE_SSL)
  } else if (state_ == STATE_TLS_CONNECTING) {
    state_ = STATE_TLS_OPEN;
    LOG(INFO) << "XmppSocketAdapter::OnConnectEvent - STATE_TLS_OPEN";
    SignalSSLConnected();
    if (write_buffer_length_ > 0) {
      HandleWritable();
    }
#endif  // defined(FEATURE_ENABLE_SSL)
  } else {
    LOG(DFATAL) << "unexpected XmppSocketAdapter::OnConnectEvent state: "
                << state_;
  }
}

void XmppSocketAdapter::OnReadEvent(talk_base::AsyncSocket *socket) {
  HandleReadable();
}

void XmppSocketAdapter::OnWriteEvent(talk_base::AsyncSocket *socket) {
  HandleWritable();
}

void XmppSocketAdapter::OnCloseEvent(talk_base::AsyncSocket *socket,
                                     int error) {
  LOG(INFO) << "XmppSocketAdapter::OnCloseEvent(" << error << ")";
  SetWSAError(error);
  if (error == SOCKET_EACCES) {
    SignalAuthenticationError();  // Proxy needs authentication.
  }
  NotifyClose();
}

#if defined(FEATURE_ENABLE_SSL)
bool XmppSocketAdapter::StartTls(const std::string& verify_host_name) {
  if (state_ != STATE_OPEN) {
    SetError(ERROR_WRONGSTATE);
    return false;
  }

  state_ = STATE_TLS_CONNECTING;

  DCHECK_EQ(write_buffer_length_, 0U);

  talk_base::SSLAdapter* ssl_adapter =
      static_cast<talk_base::SSLAdapter*>(socket_);

  if (allow_unverified_certs_) {
    ssl_adapter->set_ignore_bad_cert(true);
  }

  if (ssl_adapter->StartSSL(verify_host_name.c_str(), false) != 0) {
    state_ = STATE_OPEN;
    SetError(ERROR_SSL);
    return false;
  }

  return true;
}
#endif  // defined(FEATURE_ENABLE_SSL)

void XmppSocketAdapter::QueueWriteData(const char* data, size_t len) {
  // Expand buffer if needed.
  if (write_buffer_length_ + len > write_buffer_capacity_) {
    size_t new_capacity = 1024;
    while (new_capacity < write_buffer_length_ + len) {
      new_capacity = new_capacity * 2;
    }
    char* new_buffer = new char[new_capacity];
    DCHECK_LE(write_buffer_length_, 64000U);
    memcpy(new_buffer, write_buffer_, write_buffer_length_);
    delete[] write_buffer_;
    write_buffer_ = new_buffer;
    write_buffer_capacity_ = new_capacity;
  }

  // Copy data into the end of buffer.
  memcpy(write_buffer_ + write_buffer_length_, data, len);
  write_buffer_length_ += len;
}

void XmppSocketAdapter::FlushWriteQueue(Error* error, int* wsa_error) {
  DCHECK(error);
  DCHECK(wsa_error);

  size_t flushed = 0;
  while (flushed < write_buffer_length_) {
    int sent = socket_->Send(write_buffer_ + flushed,
                             static_cast<int>(write_buffer_length_ - flushed));
    if (sent < 0) {
      if (!socket_->IsBlocking()) {
        *error = ERROR_WINSOCK;
        *wsa_error = socket_->GetError();
      }
      break;
    }
    flushed += static_cast<size_t>(sent);
  }

  // Remove flushed memory.
  write_buffer_length_ -= flushed;
  memmove(write_buffer_, write_buffer_ + flushed, write_buffer_length_);

  // When everything is flushed, deallocate the buffer if it's gotten big.
  if (write_buffer_length_ == 0) {
    if (write_buffer_capacity_ > 8192) {
      delete[] write_buffer_;
      write_buffer_ = NULL;
      write_buffer_capacity_ = 0;
    }
  }
}

void XmppSocketAdapter::SetError(Error error) {
  if (error_ == ERROR_NONE) {
    error_ = error;
  }
}

void XmppSocketAdapter::SetWSAError(int error) {
  if (error_ == ERROR_NONE && error != 0) {
    error_ = ERROR_WINSOCK;
    wsa_error_ = error;
  }
}

bool XmppSocketAdapter::HandleReadable() {
  if (!IsOpen())
    return false;

  SignalRead();
  return true;
}

bool XmppSocketAdapter::HandleWritable() {
  if (!IsOpen())
    return false;

  Error error = ERROR_NONE;
  int wsa_error = 0;
  FlushWriteQueue(&error, &wsa_error);
  if (error != ERROR_NONE) {
    Close();
    return false;
  }
  return true;
}

}  // namespace notifier
