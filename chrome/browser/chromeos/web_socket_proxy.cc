// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_socket_proxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/web_socket_proxy_helper.h"
#include "chrome/browser/internal_auth.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_parse.h"
#include "net/base/address_list.h"
#include "net/base/cert_verifier.h"
#include "net/base/host_port_pair.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/stream_socket.h"
#include "third_party/libevent/evdns.h"
#include "third_party/libevent/event.h"

using content::BrowserThread;

namespace chromeos {

namespace {

const uint8 kCRLF[] = "\r\n";
const uint8 kCRLFCRLF[] = "\r\n\r\n";

// Not a constant but preprocessor definition for easy concatenation.
#define kProxyPath "/tcpproxy"

enum WebSocketStatusCode {
  WS_CLOSE_NORMAL = 1000,
  WS_CLOSE_GOING_AWAY = 1001,
  WS_CLOSE_PROTOCOL_ERROR = 1002,
  WS_CLOSE_UNACCEPTABLE_DATA = 1003,

  WS_CLOSE_DESTINATION_ERROR = 4000,
  WS_CLOSE_LIMIT_VIOLATION = 4001,
  WS_CLOSE_RESOLUTION_FAILED = 4002,
  WS_CLOSE_UNEXPECTED = 4003
};

enum WebSocketFrameOpcode {
  WS_OPCODE_TEXT = 1,
  WS_OPCODE_CLOSE = 8
};

// Fixed-size (one-byte) messages communicated via control pipe.
const char kControlMessageShutdown[] = { '.' };
const char kControlMessageNetworkChange[] = { ':' };

// Returns true on success.
bool SetNonBlock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

// Returns true on success.
bool IgnoreSigPipe() {
  struct sigaction sa;
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  if (sigemptyset(&sa.sa_mask) || sigaction(SIGPIPE, &sa, 0)) {
    LOG(ERROR) << "WebSocketProxy: Failed to disable sigpipe";
    return false;
  }
  return true;
}

uint64 ReadNetworkInteger(uint8* buf, int num_bytes) {
  uint64 rv = 0;
  DCHECK_GE(num_bytes, 0);
  DCHECK_LE(num_bytes, 8);
  while (num_bytes--) {
    rv <<= 8;
    rv += *buf++;
  }
  return rv;
}

void WriteNetworkInteger(int64 n, uint8* buf, int num_bytes) {
  DCHECK_GE(num_bytes, 0);
  DCHECK_LE(num_bytes, 8);
  while (num_bytes--) {
    buf[num_bytes] = n % (1 << 8);
    n >>= 8;
  }
}

typedef uint8 (*AsciiFilter)(uint8);

uint8 AsciiFilterVerbatim(uint8 c) {
  return c;
}

uint8 AsciiFilterLower(uint8 c) {
  return base::ToLowerASCII(c);
}

std::string FetchAsciiSnippet(uint8* begin, uint8* end, AsciiFilter filter) {
  std::string rv;
  for (; begin < end; ++begin) {
    if (!isascii(*begin))
      return rv;
    rv += filter(*begin);
  }
  return rv;
}

std::string FetchExtensionIdFromOrigin(const std::string &origin) {
  GURL url(origin);
  if (url.SchemeIs(chrome::kExtensionScheme))
    return url.host();
  else
    return std::string();
}

inline size_t strlen(const void* s) {
  return ::strlen(static_cast<const char*>(s));
}

void SendNotification(int port) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
      content::NotificationService::AllSources(), content::Details<int>(&port));
}

class Conn;

// Websocket to TCP proxy server.
class Serv {
 public:
  explicit Serv(const std::vector<std::string>& allowed_origins);
  ~Serv();

  // Do not call it twice.
  void Run();

  // Terminates running server (should be called on a different thread).
  void Shutdown();
  // Called on network change.  Reinitializes DNS resolution service.
  void OnNetworkChange();

  void ZapConn(Conn*);
  void MarkConnImportance(Conn*, bool important);
  Conn* GetFreshConn();
  bool IsConnSane(Conn*);
  bool IsOriginAllowed(const std::string& origin);
  void CloseAll();

  static void OnConnect(int listening_sock, short event, void*);
  static void OnControlRequest(int fd, short event, void*);

  struct event_base* evbase() { return evbase_; }

  // Checked against value of Origin field specified
  // in a client websocket handshake.
  std::vector<std::string> allowed_origins_;

  // Libevent base.
  struct event_base* evbase_;

  // Socket to listen incoming websocket connections.
  int listening_sock_;

  // TODO(dilmah): remove this extra socket as soon as obsolete
  // getPassportForTCP function is removed from webSocketProxyPrivate API.
  // Additional socket to listen incoming connections on fixed port 10101.
  int extra_listening_sock_;

  // Used to communicate control requests: either shutdown request or network
  // change notification.
  int control_descriptor_[2];

  // Flag whether shutdown has been requested.
  bool shutdown_requested_;

  // List of pending connections; We are trying to keep size of this list
  // below kConnPoolLimit in LRU fashion.
  typedef std::list<Conn*> ConnPool;
  ConnPool conn_pool_;

  // Reverse map to look up a connection in a conn_pool.
  typedef std::map<Conn*, ConnPool::iterator> RevMap;
  RevMap rev_map_;

  scoped_ptr<struct event> connection_event_;
  scoped_ptr<struct event> control_event_;
  // TODO(dilmah): remove this extra event as soon as obsolete
  // getPassportForTCP function is removed from webSocketProxyPrivate API.
  scoped_ptr<struct event> extra_connection_event_;

  DISALLOW_COPY_AND_ASSIGN(Serv);
};

// Connection (amalgamates both channels between proxy and javascript and
// between proxy and destination).
class Conn {
 public:
  enum Phase {
    // Initial stage of connection.
    PHASE_WAIT_HANDSHAKE,
    PHASE_WAIT_DESTFRAME,
    PHASE_WAIT_DESTCONNECT,

    // Operational stage of connection.
    PHASE_OUTSIDE_FRAME,
    PHASE_INSIDE_FRAME_BASE64,
    PHASE_INSIDE_FRAME_SKIP,

    // Terminal stage of connection.
    PHASE_SHUT,    // Closing handshake was emitted, buffers may be pending.
    PHASE_DEFUNCT  // Connection was nuked.
  };

  // Channel structure (either proxy<->browser or proxy<->destination).
  class Chan {
   public:
    explicit Chan(Conn* master)
        : master_(master),
          write_pending_(false),
          read_bev_(NULL),
          write_bev_(NULL),
          read_fd_(-1),
          write_fd_(-1) {
    }

    ~Chan() {
      Zap();
    }

    // Returns true on success.
    bool Write(const void* data, size_t size) {
      if (write_bev_ == NULL)
        return false;
      write_pending_ = true;
      return (0 == bufferevent_write(write_bev_, data, size));
    }

    void Zap() {
      if (read_bev_) {
        bufferevent_disable(read_bev_, EV_READ);
        bufferevent_free(read_bev_);
      }
      if (write_bev_ && write_bev_ != read_bev_) {
        bufferevent_disable(write_bev_, EV_READ);
        bufferevent_free(write_bev_);
      }
      read_bev_ = NULL;
      write_bev_ = NULL;
      if (write_fd_ && read_fd_ == write_fd_)
        shutdown(write_fd_, SHUT_RDWR);
      if (write_fd_ >= 0) {
        close(write_fd_);
        DCHECK_GE(read_fd_, 0);
      }
      if (read_fd_ && read_fd_ != write_fd_)
        close(read_fd_);
      read_fd_ = -1;
      write_fd_ = -1;
      write_pending_ = false;
      master_->ConsiderSuicide();
    }

    void Shut() {
      if (!write_pending_)
        Zap();
    }

    int read_fd() const { return read_fd_; }
    void set_read_fd(int fd) { read_fd_ = fd; }
    int write_fd() const { return write_fd_; }
    void set_write_fd(int fd) { write_fd_ = fd; }
    bool write_pending() const { return write_pending_; }
    void set_write_pending(bool pending) { write_pending_ = pending; }
    struct bufferevent* read_bev() const { return read_bev_; }
    void set_read_bev(struct bufferevent* bev) { read_bev_ = bev; }
    struct bufferevent* write_bev() const { return write_bev_; }
    void set_write_bev(struct bufferevent* bev) { write_bev_ = bev; }

   private:
    Conn* master_;
    bool write_pending_;  // Whether write buffer is not flushed yet.
    struct bufferevent* read_bev_;
    struct bufferevent* write_bev_;
    // UNIX descriptors.
    int read_fd_;
    int write_fd_;

    DISALLOW_COPY_AND_ASSIGN(Chan);
  };

  // Status of processing incoming data.
  enum Status {
    STATUS_OK,
    STATUS_INCOMPLETE,  // Not all required data is present in buffer yet.
    STATUS_SKIP,
    STATUS_ABORT        // Data is invalid.  We must shut connection.
  };

  // Unfortunately evdns callbacks are uncancellable,
  // so potentially we can receive callback for a deleted Conn.
  // Even worse, storage of deleted Conn may be reused
  // for a new connection and new connection can receive callback
  // destined for deleted Conn.
  // EventKey is introduced in order to prevent that.
  typedef void* EventKey;
  typedef std::map<EventKey, Conn*> EventKeyMap;

  explicit Conn(Serv* master);
  ~Conn();

  static Conn* Get(EventKey evkey);

  void Shut(int status, const void* reason);

  void ConsiderSuicide();

  Status ConsumeHeader(struct evbuffer*);
  Status ConsumeDestframe(struct evbuffer*);
  Status ConsumeFrameHeader(struct evbuffer*);
  Status ProcessFrameData(struct evbuffer*);

  // Return true on success.
  bool EmitHandshake(Chan*);
  bool EmitFrame(
      Chan*, WebSocketFrameOpcode opcode, const void* buf, size_t size);

  // Attempts to establish second connection (to remote TCP service).
  // Returns true on success.
  bool TryConnectDest(const struct sockaddr*, socklen_t);

  // Return security origin associated with this connection.
  const std::string& GetOrigin();

  // Used as libevent callbacks.
  static void OnDestConnectTimeout(int, short, EventKey);
  static void OnPrimchanRead(struct bufferevent*, EventKey);
  static void OnPrimchanWrite(struct bufferevent*, EventKey);
  static void OnPrimchanError(struct bufferevent*, short what, EventKey);
  static void OnDestResolutionIPv4(int result, char type, int count,
                                   int ttl, void* addr_list, EventKey);
  static void OnDestResolutionIPv6(int result, char type, int count,
                                   int ttl, void* addr_list, EventKey);
  static void OnDestchanRead(struct bufferevent*, EventKey);
  static void OnDestchanWrite(struct bufferevent*, EventKey);
  static void OnDestchanError(struct bufferevent*, short what, EventKey);

  Chan& primchan() { return primchan_; }
  EventKey evkey() const { return evkey_; }

 private:
  Serv* master_;
  Phase phase_;
  uint64 frame_bytes_remaining_;
  uint8 frame_mask_[4];
  int frame_mask_index_;

  // We maintain two channels per Conn:
  // primary channel is websocket connection.
  Chan primchan_;
  // Destination channel is a proxied connection.
  Chan destchan_;

  EventKey evkey_;

  // Header fields supplied by client at initial websocket handshake.
  std::map<std::string, std::string> header_fields_;

  // Parameters requested via query component of GET resource.
  std::map<std::string, std::string> requested_parameters_;

  // Hostname and port of destination socket.
  // Websocket client supplies them in first data frame (destframe).
  std::string destname_;
  int destport_;

  // Preresolved |destname_| (empty if not pre-resolved).
  std::string destaddr_;

  // Whether TLS over TCP requested.
  bool do_tls_;

  // We try to DNS resolve hostname in both IPv4 and IPv6 domains.
  // Track resolution failures here.
  bool destresolution_ipv4_failed_;
  bool destresolution_ipv6_failed_;

  // Used to schedule a timeout for initial phase of connection.
  scoped_ptr<struct event> destconnect_timeout_event_;

  static base::LazyInstance<EventKeyMap,
                            base::LeakyLazyInstanceTraits<EventKeyMap> >
      evkey_map_;
  static EventKey last_evkey_;

  DISALLOW_COPY_AND_ASSIGN(Conn);
};

class SSLChan : public MessageLoopForIO::Watcher {
 public:
  static void Start(const net::AddressList& address_list,
                    const net::HostPortPair& host_port_pair,
                    int read_pipe,
                    int write_pipe) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    SSLChan* ALLOW_UNUSED chan = new SSLChan(
        address_list, host_port_pair, read_pipe, write_pipe);
  }

 private:
  enum Phase {
    PHASE_CONNECTING,
    PHASE_RUNNING,
    PHASE_CLOSING,
    PHASE_CLOSED
  };

  class DerivedIOBufferWithSize : public net::IOBufferWithSize {
   public:
    DerivedIOBufferWithSize(net::IOBuffer* host, int size)
        : IOBufferWithSize(host->data(), size), host_(host) {
      DCHECK(host_);
      DCHECK(host_->data());
    }

    virtual ~DerivedIOBufferWithSize() {
      data_ = NULL;  // We do not own memory, bypass base class destructor.
    }

   protected:
    scoped_refptr<net::IOBuffer> host_;
  };

  // Provides queue of data represented as IOBuffers.
  class IOBufferQueue {
   public:
    // We do not allocate all capacity at once but lazily in |buf_size_| chunks.
    explicit IOBufferQueue(int capacity)
        : buf_size_(1 + capacity / kNumBuffersLimit) {
    }

    // Obtains IOBuffer to add new data to back.
    net::IOBufferWithSize* GetIOBufferToFill() {
      if (back_ == NULL) {
        if (storage_.size() >= kNumBuffersLimit)
          return NULL;
        storage_.push_back(new net::IOBufferWithSize(buf_size_));
        back_ = new net::DrainableIOBuffer(storage_.back(), buf_size_);
      }
      return new DerivedIOBufferWithSize(
          back_.get(), back_->BytesRemaining());
    }

    // Obtains IOBuffer with some data from front.
    net::IOBufferWithSize* GetIOBufferToProcess() {
      if (front_ == NULL) {
        if (storage_.empty())
          return NULL;
        front_ = new net::DrainableIOBuffer(storage_.front(), buf_size_);
      }
      int front_capacity = (storage_.size() == 1 && back_) ?
          back_->BytesConsumed() : buf_size_;
      return new DerivedIOBufferWithSize(
          front_.get(), front_capacity - front_->BytesConsumed());
    }

    // Records number of bytes as added to back.
    void DidFill(int bytes) {
      DCHECK(back_);
      back_->DidConsume(bytes);
      if (back_->BytesRemaining() == 0)
        back_ = NULL;
    }

    // Pops number of bytes from front.
    void DidProcess(int bytes) {
      DCHECK(front_);
      front_->DidConsume(bytes);
      if (front_->BytesRemaining() == 0) {
        storage_.pop_front();
        front_ = NULL;
      }
    }

    void Clear() {
      front_ = NULL;
      back_ = NULL;
      storage_.clear();
    }

   private:
    static const unsigned kNumBuffersLimit = 12;
    const int buf_size_;
    std::list< scoped_refptr<net::IOBufferWithSize> > storage_;
    scoped_refptr<net::DrainableIOBuffer> front_;
    scoped_refptr<net::DrainableIOBuffer> back_;

    DISALLOW_COPY_AND_ASSIGN(IOBufferQueue);
  };

  SSLChan(const net::AddressList address_list,
          const net::HostPortPair host_port_pair,
          int read_pipe,
          int write_pipe)
      : phase_(PHASE_CONNECTING),
        host_port_pair_(host_port_pair),
        inbound_stream_(WebSocketProxy::kBufferLimit),
        outbound_stream_(WebSocketProxy::kBufferLimit),
        read_pipe_(read_pipe),
        write_pipe_(write_pipe),
        method_factory_(this),
        socket_connect_callback_(NewCallback(this, &SSLChan::OnSocketConnect)),
        ssl_handshake_callback_(
            NewCallback(this, &SSLChan::OnSSLHandshakeCompleted)),
        socket_read_callback_(NewCallback(this, &SSLChan::OnSocketRead)),
        socket_write_callback_(NewCallback(this, &SSLChan::OnSocketWrite)) {
    if (!SetNonBlock(read_pipe_) || !SetNonBlock(write_pipe_)) {
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    net::ClientSocketFactory* factory =
        net::ClientSocketFactory::GetDefaultFactory();
    socket_.reset(factory->CreateTransportClientSocket(
        address_list, NULL, net::NetLog::Source()));
    if (socket_ == NULL) {
      Shut(net::ERR_FAILED);
      return;
    }
    int result = socket_->Connect(socket_connect_callback_.get());
    if (result != net::ERR_IO_PENDING)
      OnSocketConnect(result);
  }

  ~SSLChan() {
    phase_ = PHASE_CLOSED;
    write_pipe_controller_.StopWatchingFileDescriptor();
    read_pipe_controller_.StopWatchingFileDescriptor();
    close(write_pipe_);
    close(read_pipe_);
  }

  void Shut(int ALLOW_UNUSED net_error_code) {
    if (phase_ != PHASE_CLOSED) {
      phase_ = PHASE_CLOSING;
      scoped_refptr<net::IOBufferWithSize> buf[] = {
          outbound_stream_.GetIOBufferToProcess(),
          inbound_stream_.GetIOBufferToProcess()
      };
      for (int i = arraysize(buf); i--;) {
        if (buf[i] && buf[i]->size() > 0) {
          MessageLoop::current()->PostTask(FROM_HERE,
              method_factory_.NewRunnableMethod(&SSLChan::Proceed));
          return;
        }
      }
      phase_ = PHASE_CLOSED;
      if (socket_ != NULL) {
        socket_->Disconnect();
        socket_.reset();
      }
      MessageLoop::current()->DeleteSoon(FROM_HERE, this);
    }
  }

  void OnSocketConnect(int result) {
    if (phase_ != PHASE_CONNECTING) {
      NOTREACHED();
      return;
    }
    if (result) {
      Shut(result);
      return;
    }
    net::ClientSocketHandle* handle = new net::ClientSocketHandle();
    handle->set_socket(socket_.release());
    net::ClientSocketFactory* factory =
        net::ClientSocketFactory::GetDefaultFactory();
    net::SSLClientSocketContext ssl_context;
    if (!cert_verifier_.get())
      cert_verifier_.reset(new net::CertVerifier());
    ssl_context.cert_verifier = cert_verifier_.get();
    socket_.reset(factory->CreateSSLClientSocket(
        handle, host_port_pair_, ssl_config_, NULL, ssl_context));
    if (!socket_.get()) {
      LOG(WARNING) << "Failed to create an SSL client socket.";
      OnSSLHandshakeCompleted(net::ERR_UNEXPECTED);
      return;
    }
    result = socket_->Connect(ssl_handshake_callback_.get());
    if (result != net::ERR_IO_PENDING)
      OnSSLHandshakeCompleted(result);
  }

  void OnSSLHandshakeCompleted(int result) {
    if (result) {
      Shut(result);
      return;
    }
    is_socket_read_pending_ = false;
    is_socket_write_pending_ = false;
    is_read_pipe_blocked_ = false;
    is_write_pipe_blocked_ = false;
    MessageLoopForIO::current()->WatchFileDescriptor(
        read_pipe_, false, MessageLoopForIO::WATCH_READ,
        &read_pipe_controller_, this);
    MessageLoopForIO::current()->WatchFileDescriptor(
        write_pipe_, false, MessageLoopForIO::WATCH_WRITE,
        &write_pipe_controller_, this);
    phase_ = PHASE_RUNNING;
    Proceed();
  }

  void OnSocketRead(int result) {
    DCHECK(is_socket_read_pending_);
    is_socket_read_pending_ = false;
    if (result <= 0) {
      Shut(result);
      return;
    }
    inbound_stream_.DidFill(result);
    Proceed();
  }

  void OnSocketWrite(int result) {
    DCHECK(is_socket_write_pending_);
    is_socket_write_pending_ = false;
    if (result < 0) {
      outbound_stream_.Clear();
      Shut(result);
      return;
    }
    outbound_stream_.DidProcess(result);
    Proceed();
  }

  // MessageLoopForIO::Watcher overrides.
  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    if (fd != read_pipe_) {
      NOTREACHED();
      return;
    }
    is_read_pipe_blocked_ = false;
    Proceed();
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    if (fd != write_pipe_) {
      NOTREACHED();
      return;
    }
    is_write_pipe_blocked_ = false;
    Proceed();
  }

 private:
  void Proceed() {
    if (phase_ != PHASE_RUNNING && phase_ != PHASE_CLOSING)
      return;
    for (bool proceed = true; proceed;) {
      proceed = false;
      if (!is_read_pipe_blocked_ && phase_ == PHASE_RUNNING) {
        scoped_refptr<net::IOBufferWithSize> buf =
            outbound_stream_.GetIOBufferToFill();
        if (buf && buf->size() > 0) {
          int rv = read(read_pipe_, buf->data(), buf->size());
          if (rv > 0) {
            outbound_stream_.DidFill(rv);
            proceed = true;
          } else if (rv == -1 && errno == EAGAIN) {
            is_read_pipe_blocked_ = true;
            MessageLoopForIO::current()->WatchFileDescriptor(
                read_pipe_, false, MessageLoopForIO::WATCH_READ,
                &read_pipe_controller_, this);
          } else if (rv == 0) {
            Shut(0);
          } else {
            DCHECK_LT(rv, 0);
            Shut(net::ERR_UNEXPECTED);
            return;
          }
        }
      }
      if (!is_socket_read_pending_ && phase_ == PHASE_RUNNING) {
        scoped_refptr<net::IOBufferWithSize> buf =
            inbound_stream_.GetIOBufferToFill();
        if (buf && buf->size() > 0) {
          int rv = socket_->Read(buf, buf->size(), socket_read_callback_.get());
          is_socket_read_pending_ = true;
          if (rv != net::ERR_IO_PENDING) {
            MessageLoop::current()->PostTask(FROM_HERE,
                method_factory_.NewRunnableMethod(&SSLChan::OnSocketRead, rv));
          }
        }
      }
      if (!is_socket_write_pending_) {
        scoped_refptr<net::IOBufferWithSize> buf =
            outbound_stream_.GetIOBufferToProcess();
        if (buf && buf->size() > 0) {
          int rv = socket_->Write(
              buf, buf->size(), socket_write_callback_.get());
          is_socket_write_pending_ = true;
          if (rv != net::ERR_IO_PENDING) {
            MessageLoop::current()->PostTask(FROM_HERE,
                method_factory_.NewRunnableMethod(&SSLChan::OnSocketWrite, rv));
          }
        } else if (phase_ == PHASE_CLOSING) {
          Shut(0);
        }
      }
      if (!is_write_pipe_blocked_) {
        scoped_refptr<net::IOBufferWithSize> buf =
            inbound_stream_.GetIOBufferToProcess();
        if (buf && buf->size() > 0) {
          int rv = write(write_pipe_, buf->data(), buf->size());
          if (rv > 0) {
            inbound_stream_.DidProcess(rv);
            proceed = true;
          } else if (rv == -1 && errno == EAGAIN) {
            is_write_pipe_blocked_ = true;
            MessageLoopForIO::current()->WatchFileDescriptor(
                write_pipe_, false, MessageLoopForIO::WATCH_WRITE,
                &write_pipe_controller_, this);
          } else {
            DCHECK_LE(rv, 0);
            inbound_stream_.Clear();
            Shut(net::ERR_UNEXPECTED);
            return;
          }
        } else if (phase_ == PHASE_CLOSING) {
          Shut(0);
        }
      }
    }
  }

  Phase phase_;
  scoped_ptr<net::StreamSocket> socket_;
  net::HostPortPair host_port_pair_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
  net::SSLConfig ssl_config_;
  IOBufferQueue inbound_stream_;
  IOBufferQueue outbound_stream_;
  int read_pipe_;
  int write_pipe_;
  bool is_socket_read_pending_;
  bool is_socket_write_pending_;
  bool is_read_pipe_blocked_;
  bool is_write_pipe_blocked_;
  ScopedRunnableMethodFactory<SSLChan> method_factory_;
  scoped_ptr<net::OldCompletionCallback> socket_connect_callback_;
  scoped_ptr<net::OldCompletionCallback> ssl_handshake_callback_;
  scoped_ptr<net::OldCompletionCallback> socket_read_callback_;
  scoped_ptr<net::OldCompletionCallback> socket_write_callback_;
  MessageLoopForIO::FileDescriptorWatcher read_pipe_controller_;
  MessageLoopForIO::FileDescriptorWatcher write_pipe_controller_;

  friend class DeleteTask<SSLChan>;
  DISALLOW_COPY_AND_ASSIGN(SSLChan);
};

Serv::Serv(const std::vector<std::string>& allowed_origins)
    : allowed_origins_(allowed_origins),
      evbase_(NULL),
      listening_sock_(-1),
      extra_listening_sock_(-1),
      shutdown_requested_(false) {
  std::sort(allowed_origins_.begin(), allowed_origins_.end());
  control_descriptor_[0] = -1;
  control_descriptor_[1] = -1;
}

Serv::~Serv() {
  CloseAll();
}

void Serv::Run() {
  if (evbase_ || shutdown_requested_)
    return;

  evbase_ = event_init();
  if (!evbase_) {
    LOG(ERROR) << "WebSocketProxy: Couldn't create libevent base";
    return;
  }

  if (pipe(control_descriptor_) ||
      !SetNonBlock(control_descriptor_[0]) ||
      !SetNonBlock(control_descriptor_[1])) {
    LOG(ERROR) << "WebSocketProxy: Failed to create control pipe";
    return;
  }

  listening_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listening_sock_ < 0) {
    LOG(ERROR) << "WebSocketProxy: Failed to create socket";
    return;
  }
  {
    int on = 1;
    setsockopt(listening_sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(0);  // let OS allocatate ephemeral port number.
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (bind(listening_sock_,
           reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr))) {
    LOG(ERROR) << "WebSocketProxy: Failed to bind server socket";
    return;
  }
  if (listen(listening_sock_, 12)) {
    LOG(ERROR) << "WebSocketProxy: Failed to listen server socket";
    return;
  }
  if (!SetNonBlock(listening_sock_)) {
    LOG(ERROR) << "WebSocketProxy: Failed to go non block";
    return;
  }

  connection_event_.reset(new struct event);
  event_set(connection_event_.get(), listening_sock_, EV_READ | EV_PERSIST,
            &OnConnect, this);
  event_base_set(evbase_, connection_event_.get());
  if (event_add(connection_event_.get(), NULL)) {
    LOG(ERROR) << "WebSocketProxy: Failed to add listening event";
    return;
  }

  {
    // TODO(dilmah): remove this control block as soon as obsolete
    // getPassportForTCP function is removed from webSocketProxyPrivate API.
    // Following block adds extra listening socket on fixed port 10101.
    extra_listening_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (extra_listening_sock_ < 0) {
      LOG(ERROR) << "WebSocketProxy: Failed to create socket";
      return;
    }
    {
      int on = 1;
      setsockopt(listening_sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    }
    const int kPort = 10101;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(extra_listening_sock_,
             reinterpret_cast<struct sockaddr*>(&addr),
             sizeof(addr))) {
      LOG(ERROR) << "WebSocketProxy: Failed to bind server socket";
      return;
    }
    if (listen(extra_listening_sock_, 12)) {
      LOG(ERROR) << "WebSocketProxy: Failed to listen server socket";
      return;
    }
    if (!SetNonBlock(extra_listening_sock_)) {
      LOG(ERROR) << "WebSocketProxy: Failed to go non block";
      return;
    }
    extra_connection_event_.reset(new struct event);
    event_set(extra_connection_event_.get(), extra_listening_sock_,
              EV_READ | EV_PERSIST, &OnConnect, this);
    event_base_set(evbase_, extra_connection_event_.get());
    if (event_add(extra_connection_event_.get(), NULL)) {
      LOG(ERROR) << "WebSocketProxy: Failed to add listening event";
      return;
    }
  }

  control_event_.reset(new struct event);
  event_set(control_event_.get(), control_descriptor_[0], EV_READ | EV_PERSIST,
            &OnControlRequest, this);
  event_base_set(evbase_, control_event_.get());
  if (event_add(control_event_.get(), NULL)) {
    LOG(ERROR) << "WebSocketProxy: Failed to add control event";
    return;
  }

  if (evdns_init())
    LOG(WARNING) << "WebSocketProxy: Failed to initialize evDNS";
  if (!IgnoreSigPipe()) {
    LOG(ERROR) << "WebSocketProxy: Failed to ignore SIGPIPE";
    return;
  }

  memset(&addr, 0, sizeof(addr));
  socklen_t addr_len = sizeof(addr);
  if (getsockname(
      listening_sock_, reinterpret_cast<struct sockaddr*>(&addr), &addr_len)) {
    LOG(ERROR) << "Failed to determine listening port";
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&SendNotification, ntohs(addr.sin_port)));

  LOG(INFO) << "WebSocketProxy: Starting event dispatch loop.";
  event_base_dispatch(evbase_);
  if (shutdown_requested_)
    LOG(INFO) << "WebSocketProxy: Event dispatch loop terminated upon request";
  else
    LOG(ERROR) << "WebSocketProxy: Event dispatch loop terminated unexpectedly";
  CloseAll();
}

void Serv::Shutdown() {
  int r ALLOW_UNUSED =
      write(control_descriptor_[1], kControlMessageShutdown, 1);
}

void Serv::OnNetworkChange() {
  int r ALLOW_UNUSED =
      write(control_descriptor_[1], kControlMessageNetworkChange, 1);
}

void Serv::CloseAll() {
  while (!conn_pool_.empty())
    ZapConn(conn_pool_.back());
  if (listening_sock_ >= 0) {
    shutdown(listening_sock_, SHUT_RDWR);
    close(listening_sock_);
  }
  for (int i = 0; i < 2; ++i) {
    if (control_descriptor_[i] >= 0) {
      control_descriptor_[i] = -1;
      close(control_descriptor_[i]);
    }
  }
  if (control_event_.get()) {
    event_del(control_event_.get());
    control_event_.reset();
  }
  if (extra_connection_event_.get()) {
    event_del(extra_connection_event_.get());
    extra_connection_event_.reset();
  }
  if (connection_event_.get()) {
    event_del(connection_event_.get());
    connection_event_.reset();
  }
  if (evbase_) {
    event_base_free(evbase_);
    evbase_ = NULL;
  }
}

void Serv::ZapConn(Conn* cs) {
  RevMap::iterator rit = rev_map_.find(cs);
  if (rit != rev_map_.end()) {
    conn_pool_.erase(rit->second);
    rev_map_.erase(rit);
    delete cs;
  }
}

void Serv::MarkConnImportance(Conn* cs, bool important) {
  if (conn_pool_.size() < WebSocketProxy::kConnPoolLimit / 4) {
    // Fast common path.
    return;
  }
  RevMap::iterator rit = rev_map_.find(cs);
  if (rit != rev_map_.end()) {
    ConnPool::iterator it = rit->second;
    CHECK(*it == cs);
    if (important && it == conn_pool_.begin()) {
      // Already at the top.  Shortcut.
      return;
    }
    conn_pool_.erase(it);
  }
  if (important) {
    conn_pool_.push_front(cs);
    rev_map_[cs] = conn_pool_.begin();
  } else {
    conn_pool_.push_back(cs);
    rev_map_[cs] = conn_pool_.end();
    --rev_map_[cs];
  }
}

Conn* Serv::GetFreshConn() {
  if (conn_pool_.size() > WebSocketProxy::kConnPoolLimit) {
    // Connections overflow.  Shut those oldest not active.
    ConnPool::iterator it = conn_pool_.end();
    --it;
    for (int i = conn_pool_.size() - WebSocketProxy::kConnPoolLimit; i-- > 0;) {
      // Shut may invalidate an iterator; hence postdecrement.
      (*it--)->Shut(WS_CLOSE_GOING_AWAY,
                    "Flood of new connections, getting rid of old ones");
    }
    if (conn_pool_.size() > WebSocketProxy::kConnPoolLimit + 12) {
      // Connections overflow.  Zap the oldest not active.
      ZapConn(conn_pool_.back());
    }
  }
  Conn* cs = new Conn(this);
  conn_pool_.push_front(cs);
  rev_map_[cs] = conn_pool_.begin();
  return cs;
}

bool Serv::IsConnSane(Conn* cs) {
  return rev_map_.find(cs) != rev_map_.end();
}

bool Serv::IsOriginAllowed(const std::string& origin) {
  return allowed_origins_.empty() || std::binary_search(
      allowed_origins_.begin(), allowed_origins_.end(), origin);
}

// static
void Serv::OnConnect(int listening_sock, short event, void* ctx) {
  Serv* self = static_cast<Serv*>(ctx);
  Conn* cs = self->GetFreshConn();
  int sock = accept(listening_sock, NULL, NULL);
  if (sock < 0 || !SetNonBlock(sock)) {
    // Read readiness was triggered on listening socket
    // yet we failed to accept a connection; definitely weird.
    NOTREACHED();
    self->ZapConn(cs);
    return;
  }
  cs->primchan().set_read_fd(sock);
  cs->primchan().set_write_fd(sock);

  struct bufferevent* bev = bufferevent_new(
      sock,
      &Conn::OnPrimchanRead, &Conn::OnPrimchanWrite, &Conn::OnPrimchanError,
      cs->evkey());
  if (bev == NULL) {
    self->ZapConn(cs);
    return;
  }
  cs->primchan().set_read_bev(bev);
  cs->primchan().set_write_bev(bev);
  bufferevent_base_set(self->evbase_, bev);
  bufferevent_setwatermark(bev, EV_READ, 0, WebSocketProxy::kBufferLimit);
  if (bufferevent_enable(bev, EV_READ | EV_WRITE)) {
    self->ZapConn(cs);
    return;
  }
}

// static
void Serv::OnControlRequest(int fd, short event, void* ctx) {
  Serv* self = static_cast<Serv*>(ctx);

  char c;
  if (1 == read(fd, &c, 1) && c == *kControlMessageNetworkChange) {
    // OnNetworkChange request.
    evdns_clear_nameservers_and_suspend();
    evdns_init();
    evdns_resume();
  } else if (c == *kControlMessageShutdown) {
    self->shutdown_requested_ = true;
    event_base_loopbreak(self->evbase_);
  }
}

Conn::Conn(Serv* master)
    : master_(master),
      phase_(PHASE_WAIT_HANDSHAKE),
      frame_bytes_remaining_(0),
      frame_mask_index_(0),
      primchan_(this),
      destchan_(this),
      do_tls_(false),
      destresolution_ipv4_failed_(false),
      destresolution_ipv6_failed_(false) {
  while (evkey_map_.Get().find(last_evkey_) != evkey_map_.Get().end()) {
    last_evkey_ = reinterpret_cast<EventKey>(reinterpret_cast<size_t>(
        last_evkey_) + 1);
  }
  evkey_ = last_evkey_;
  evkey_map_.Get()[evkey_] = this;
  // Schedule timeout for initial phase of connection.
  destconnect_timeout_event_.reset(new struct event);
  evtimer_set(destconnect_timeout_event_.get(),
              &OnDestConnectTimeout, evkey_);
  event_base_set(master_->evbase(),
                 destconnect_timeout_event_.get());

  struct timeval tv;
  tv.tv_sec = 20;
  tv.tv_usec = 0;
  evtimer_add(destconnect_timeout_event_.get(), &tv);
}

Conn::~Conn() {
  phase_ = PHASE_DEFUNCT;
  event_del(destconnect_timeout_event_.get());
  if (evkey_map_.Get()[evkey_] == this)
    evkey_map_.Get().erase(evkey_);
  else
    NOTREACHED();
}

Conn* Conn::Get(EventKey evkey) {
  EventKeyMap::iterator it = evkey_map_.Get().find(evkey);
  if (it == evkey_map_.Get().end())
    return NULL;
  Conn* cs = it->second;
  if (cs == NULL ||
      cs->evkey_ != evkey ||
      cs->master_ == NULL ||
      cs->phase_ < 0 ||
      cs->phase_ > PHASE_SHUT ||
      !cs->master_->IsConnSane(cs)) {
    return NULL;
  }
  return cs;
}

void Conn::Shut(int status, const void* reason) {
  if (phase_ >= PHASE_SHUT)
    return;
  master_->MarkConnImportance(this, false);

  std::vector<uint8> payload(2 + strlen(reason));
  WriteNetworkInteger(status, vector_as_array(&payload), 2);
  memcpy(vector_as_array(&payload) + 2, reason, strlen(reason));

  EmitFrame(
      &primchan_, WS_OPCODE_CLOSE, vector_as_array(&payload), payload.size());
  primchan_.Shut();
  destchan_.Shut();
  phase_ = PHASE_SHUT;
}

void Conn::ConsiderSuicide() {
  if (!primchan_.write_pending() && !destchan_.write_pending())
    master_->ZapConn(this);
}

Conn::Status Conn::ConsumeHeader(struct evbuffer* evb) {
  uint8* buf = EVBUFFER_DATA(evb);
  size_t buf_size = EVBUFFER_LENGTH(evb);

  static const uint8 kGetPrefix[] = "GET " kProxyPath;
  static const uint8 kKeyValueDelimiter[] = ": ";

  if (buf_size <= 0)
    return STATUS_INCOMPLETE;
  if (!buf)
    return STATUS_ABORT;
  if (!std::equal(buf, buf + std::min(buf_size, strlen(kGetPrefix)),
                  kGetPrefix)) {
    // Data head does not match what is expected.
    return STATUS_ABORT;
  }

  if (buf_size >= WebSocketProxy::kHeaderLimit)
    return STATUS_ABORT;
  uint8* buf_end = buf + buf_size;
  // Handshake request must end with double CRLF.
  uint8* term_pos = std::search(buf, buf_end, kCRLFCRLF,
                                kCRLFCRLF + strlen(kCRLFCRLF));
  if (term_pos == buf_end)
    return STATUS_INCOMPLETE;
  term_pos += strlen(kCRLFCRLF);
  // First line is "GET path?query protocol" line.  If query is empty then we
  // fall back to (obsolete) way of obtaining parameters from first websocket
  // frame.  Otherwise query contains all required parameters (host, port etc).
  uint8* get_request_end = std::search(
      buf, term_pos, kCRLF, kCRLF + strlen(kCRLF));
  DCHECK(get_request_end != term_pos);
  uint8* resource_end = std::find(
      buf + strlen(kGetPrefix), get_request_end, ' ');
  if (*resource_end != ' ')
    return STATUS_ABORT;
  if (resource_end != buf + strlen(kGetPrefix)) {
    char* piece = reinterpret_cast<char*>(buf) + strlen(kGetPrefix) + 1;
    url_parse::Component query(
        0, resource_end - reinterpret_cast<uint8*>(piece));
    for (url_parse::Component key, value;
        url_parse::ExtractQueryKeyValue(piece, &query, &key, &value);) {
      if (key.len > 0) {
        requested_parameters_[std::string(piece + key.begin, key.len)] =
            net::UnescapeURLComponent(std::string(piece + value.begin,
                value.len), net::UnescapeRule::URL_SPECIAL_CHARS);
      }
    }
  }
  for (uint8* pos = get_request_end;;) {
    pos += strlen(kCRLF);
    if (term_pos - pos < static_cast<ptrdiff_t>(strlen(kCRLF)))
      return STATUS_ABORT;
    if (term_pos - pos == static_cast<ptrdiff_t>(strlen(kCRLF)))
      break;
    uint8* npos = std::search(pos, term_pos, kKeyValueDelimiter,
                              kKeyValueDelimiter + strlen(kKeyValueDelimiter));
    if (npos == term_pos)
      return STATUS_ABORT;
    std::string key = FetchAsciiSnippet(pos, npos, AsciiFilterLower);
    pos = std::search(npos += strlen(kKeyValueDelimiter), term_pos,
                      kCRLF, kCRLF + strlen(kCRLF));
    if (pos == term_pos)
      return STATUS_ABORT;
    if (!key.empty()) {
      header_fields_[key] = FetchAsciiSnippet(npos, pos,
          key == "sec-websocket-key" ? AsciiFilterVerbatim : AsciiFilterLower);
    }
  }

  // Values of Upgrade and Connection fields are hardcoded in the protocol.
  if (header_fields_["upgrade"] != "websocket" ||
      header_fields_["connection"] != "upgrade" ||
      header_fields_["sec-websocket-key"].size() != 24) {
    return STATUS_ABORT;
  }
  if (header_fields_["sec-websocket-version"] != "8" &&
      header_fields_["sec-websocket-version"] != "13") {
    return STATUS_ABORT;
  }
  // Normalize origin (e.g. leading slash).
  GURL origin = GURL(GetOrigin()).GetOrigin();
  if (!origin.is_valid())
    return STATUS_ABORT;
  if (!master_->IsOriginAllowed(origin.spec()))
    return STATUS_ABORT;

  if (!requested_parameters_.empty()) {
    destname_ = requested_parameters_["hostname"];
    int port;
    if (!base::StringToInt(requested_parameters_["port"], &port) ||
        port < 0 || port >= 1 << 16) {
      return STATUS_ABORT;
    }
    destport_ = port;
    destaddr_ = requested_parameters_["addr"];
    do_tls_ = (requested_parameters_["tls"] == "true");

    requested_parameters_["extension_id"] =
        FetchExtensionIdFromOrigin(GetOrigin());
    std::string passport(requested_parameters_["passport"]);
    requested_parameters_.erase("passport");
    if (!browser::InternalAuthVerification::VerifyPassport(
        passport, "web_socket_proxy", requested_parameters_)) {
      return STATUS_ABORT;
    }
  }

  evbuffer_drain(evb, term_pos - buf);
  return STATUS_OK;
}

bool Conn::EmitHandshake(Chan* chan) {
  std::vector<std::string> boilerplate;
  boilerplate.push_back("HTTP/1.1 101 WebSocket Protocol Handshake");
  boilerplate.push_back("Upgrade: websocket");
  boilerplate.push_back("Connection: Upgrade");

  {
    // Take care of Accept field.
    std::string word = header_fields_["sec-websocket-key"];
    word += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string accept_token;
    base::Base64Encode(base::SHA1HashString(word), &accept_token);
    boilerplate.push_back("Sec-WebSocket-Accept: " + accept_token);
  }

  boilerplate.push_back("");
  for (size_t i = 0; i < boilerplate.size(); ++i) {
    if (!chan->Write(boilerplate[i].c_str(), boilerplate[i].size()) ||
        !chan->Write(kCRLF, strlen(kCRLF))) {
      return false;
    }
  }
  return true;
}

bool Conn::EmitFrame(
    Chan* chan, WebSocketFrameOpcode opcode, const void* buf, size_t size) {
  uint8 header[10];
  int header_size = 2;
  DCHECK(chan);
  DCHECK(opcode >= 0 && opcode < 16);
  header[0] = opcode | 0x80;  // FIN bit set.
  if (size < 126) {
    header[1] = size;
  } else if (size < (1 << 16)) {
    header[1] = 126;
    WriteNetworkInteger(size, header + 2, 2);
    header_size += 2;
  } else {
    header[1] = 127;
    WriteNetworkInteger(size, header + 2, 8);
    header_size += 8;
  }
  return chan->Write(header, header_size) && chan->Write(buf, size);
}

Conn::Status Conn::ConsumeDestframe(struct evbuffer* evb) {
  if (!requested_parameters_.empty()) {
    // Parameters were already provided (and verified) in query component of
    // websocket URL.
    return STATUS_OK;
  }
  if (frame_bytes_remaining_ == 0) {
    Conn::Status rv = ConsumeFrameHeader(evb);
    if (rv != STATUS_OK)
      return rv;
    if (frame_bytes_remaining_ == 0 ||
        frame_bytes_remaining_ >= WebSocketProxy::kHeaderLimit) {
      return STATUS_ABORT;
    }
  }

  uint8* buf = EVBUFFER_DATA(evb);
  size_t buf_size = EVBUFFER_LENGTH(evb);
  if (buf_size < frame_bytes_remaining_)
    return STATUS_INCOMPLETE;
  for (size_t i = 0; i < buf_size; ++i) {
    buf[i] ^= frame_mask_[frame_mask_index_];
    frame_mask_index_ = (frame_mask_index_ + 1) % 4;
  }
  std::string passport;
  if (!WebSocketProxyHelper::FetchPassportAddrNamePort(
      buf, buf + frame_bytes_remaining_,
      &passport, &destaddr_, &destname_, &destport_)) {
    return STATUS_ABORT;
  }
  std::map<std::string, std::string> map;
  map["hostname"] = destname_;
  map["port"] = base::IntToString(destport_);
  map["extension_id"] = FetchExtensionIdFromOrigin(GetOrigin());
  if (!destaddr_.empty())
    map["addr"] = destaddr_;
  if (!browser::InternalAuthVerification::VerifyPassport(
      passport, "web_socket_proxy", map)) {
    return STATUS_ABORT;
  }

  evbuffer_drain(evb, frame_bytes_remaining_);
  frame_bytes_remaining_ = 0;
  return STATUS_OK;
}

Conn::Status Conn::ConsumeFrameHeader(struct evbuffer* evb) {
  uint8* buf = EVBUFFER_DATA(evb);
  size_t buf_size = EVBUFFER_LENGTH(evb);
  size_t header_size = 2;

  if (buf_size < header_size)
    return STATUS_INCOMPLETE;
  if (buf[0] & 0x70) {
    // We are not able to handle non-zero reserved bits.
    NOTIMPLEMENTED();
    return STATUS_ABORT;
  }
  bool fin_flag = buf[0] & 0x80;
  if (!fin_flag) {
    NOTIMPLEMENTED();
    return STATUS_ABORT;
  }
  int opcode = buf[0] & 0x0f;
  switch (opcode) {
    case WS_OPCODE_TEXT:
      break;
    case WS_OPCODE_CLOSE:
      return STATUS_ABORT;
    default:
      NOTIMPLEMENTED();
      return STATUS_ABORT;
  }

  bool mask_flag = buf[1] & 0x80;
  if (!mask_flag) {
    // Client-to-server frames must be masked.
    return STATUS_ABORT;
  }
  frame_bytes_remaining_ = buf[1] & 0x7f;
  int extra_size = 0;
  if (frame_bytes_remaining_ == 126)
    extra_size = 2;
  else if (frame_bytes_remaining_ == 127)
    extra_size = 8;
  if (buf_size < header_size + extra_size + sizeof(frame_mask_))
    return STATUS_INCOMPLETE;
  if (extra_size)
    frame_bytes_remaining_ = ReadNetworkInteger(buf + header_size, extra_size);
  header_size += extra_size;
  memcpy(frame_mask_, buf + header_size, sizeof(frame_mask_));
  header_size += sizeof(frame_mask_);
  frame_mask_index_ = 0;
  evbuffer_drain(evb, header_size);
  return STATUS_OK;
}

Conn::Status Conn::ProcessFrameData(struct evbuffer* evb) {
  uint8* buf = EVBUFFER_DATA(evb);
  size_t buf_size = EVBUFFER_LENGTH(evb);

  DCHECK_GE(frame_bytes_remaining_, 1u);
  if (frame_bytes_remaining_ < buf_size)
    buf_size = frame_bytes_remaining_;
  // base64 is encoded in chunks of 4 bytes.
  buf_size = buf_size / 4 * 4;
  if (buf_size < 1)
    return STATUS_INCOMPLETE;
  switch (phase_) {
    case PHASE_INSIDE_FRAME_BASE64: {
      for (size_t i = 0; i < buf_size; ++i) {
        buf[i] ^= frame_mask_[frame_mask_index_];
        frame_mask_index_ = (frame_mask_index_ + 1) % 4;
      }
      std::string out_bytes;
      base::Base64Decode(std::string(buf, buf + buf_size), &out_bytes);
      evbuffer_drain(evb, buf_size);
      DCHECK(destchan_.write_bev());
      if (!destchan_.Write(out_bytes.c_str(), out_bytes.size()))
        return STATUS_ABORT;
      break;
    }
    case PHASE_INSIDE_FRAME_SKIP: {
      evbuffer_drain(evb, buf_size);
      break;
    }
    default: {
      return STATUS_ABORT;
    }
  }
  frame_bytes_remaining_ -= buf_size;
  return frame_bytes_remaining_ ? STATUS_INCOMPLETE : STATUS_OK;
}

bool Conn::TryConnectDest(const struct sockaddr* addr, socklen_t addrlen) {
  if (destchan_.read_fd() >= 0 || destchan_.read_bev() != NULL)
    return false;
  if (do_tls_) {
    int fd[4];
    if (pipe(fd) || pipe(fd + 2))
      return false;
    destchan_.set_read_fd(fd[0]);
    destchan_.set_write_fd(fd[3]);
    for (int i = arraysize(fd); i--;) {
      if (!SetNonBlock(fd[i]))
        return false;
    }
    destchan_.set_read_bev(bufferevent_new(
        destchan_.read_fd(),
        &OnDestchanRead, NULL, &OnDestchanError,
        evkey_));
    destchan_.set_write_bev(bufferevent_new(
        destchan_.write_fd(),
        NULL, &OnDestchanWrite, &OnDestchanError,
        evkey_));
    net::AddressList addrlist = net::AddressList::CreateFromSockaddr(
        addr, addrlen, SOCK_STREAM, IPPROTO_TCP);
    net::HostPortPair host_port_pair(destname_, destport_);
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE, base::Bind(
            &SSLChan::Start, addrlist, host_port_pair, fd[2], fd[1]));
  } else {
    int sock = socket(addr->sa_family, SOCK_STREAM, 0);
    if (sock < 0)
      return false;
    destchan_.set_read_fd(sock);
    destchan_.set_write_fd(sock);
    if (!SetNonBlock(sock))
      return false;
    if (connect(sock, addr, addrlen)) {
      if (errno != EINPROGRESS)
        return false;
    }
    destchan_.set_read_bev(bufferevent_new(
        sock,
        &OnDestchanRead, &OnDestchanWrite, &OnDestchanError,
        evkey_));
    destchan_.set_write_bev(destchan_.read_bev());
  }
  if (destchan_.read_bev() == NULL || destchan_.write_bev() == NULL)
    return false;
  if (bufferevent_base_set(master_->evbase(), destchan_.read_bev()) ||
      bufferevent_base_set(master_->evbase(), destchan_.write_bev())) {
    return false;
  }
  bufferevent_setwatermark(
      destchan_.read_bev(), EV_READ, 0, WebSocketProxy::kBufferLimit);
  if (bufferevent_enable(destchan_.read_bev(), EV_READ) ||
      bufferevent_enable(destchan_.write_bev(), EV_WRITE)) {
    return false;
  }
  return true;
}

const std::string& Conn::GetOrigin() {
  return header_fields_[header_fields_["sec-websocket-version"] == "8" ?
      "sec-websocket-origin" : "origin"];
}

// static
void Conn::OnPrimchanRead(struct bufferevent* bev, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->primchan_.read_bev()) {
    NOTREACHED();
    return;
  }
  if (EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)) <= 0)
    return;
  cs->master_->MarkConnImportance(cs, true);
  for (;;) {
    switch (cs->phase_) {
      case PHASE_WAIT_HANDSHAKE: {
        switch (cs->ConsumeHeader(EVBUFFER_INPUT(bev))) {
          case STATUS_OK: {
            break;
          }
          case STATUS_INCOMPLETE: {
            return;
          }
          case STATUS_ABORT:
          default: {
            cs->master_->ZapConn(cs);
            return;
          }
        }
        // Header consumed OK.  Do respond.
        if (!cs->EmitHandshake(&cs->primchan_)) {
          cs->master_->ZapConn(cs);
          return;
        }
        cs->phase_ = PHASE_WAIT_DESTFRAME;
      }
      case PHASE_WAIT_DESTFRAME: {
        switch (cs->ConsumeDestframe(EVBUFFER_INPUT(bev))) {
          case STATUS_OK: {
            {
              // Unfortunately libevent as of 1.4 does not look into /etc/hosts.
              // There seems to be no easy API to perform only "local" part of
              // getaddrinfo resolution.  Hence this hack for "localhost".
              if (cs->destname_ == "localhost")
                cs->destname_ = "127.0.0.1";
            }
            if (cs->destaddr_.empty())
              cs->destaddr_ = cs->destname_;
            {
              struct sockaddr_in sa;
              memset(&sa, 0, sizeof(sa));
              sa.sin_port = htons(cs->destport_);
              if (inet_pton(sa.sin_family = AF_INET,
                            cs->destaddr_.c_str(),
                            &sa.sin_addr) == 1) {
                // valid IPv4 address supplied.
                if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa))) {
                  cs->phase_ = PHASE_WAIT_DESTCONNECT;
                  return;
                }
              }
            }
            {
              if (cs->destaddr_.size() >= 2 &&
                  cs->destaddr_[0] == '[' &&
                  cs->destaddr_[cs->destaddr_.size() - 1] == ']') {
                // Literal IPv6 address in brackets.
                cs->destaddr_ =
                    cs->destaddr_.substr(1, cs->destaddr_.size() - 2);
              }
              struct sockaddr_in6 sa;
              memset(&sa, 0, sizeof(sa));
              sa.sin6_port = htons(cs->destport_);
              if (inet_pton(sa.sin6_family = AF_INET6,
                            cs->destaddr_.c_str(),
                            &sa.sin6_addr) == 1) {
                // valid IPv6 address supplied.
                if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa))) {
                  cs->phase_ = PHASE_WAIT_DESTCONNECT;
                  return;
                }
              }
            }
            // Asynchronous DNS resolution.
            if (evdns_count_nameservers() < 1) {
              evdns_clear_nameservers_and_suspend();
              evdns_init();
              evdns_resume();
            }
            evdns_resolve_ipv4(cs->destname_.c_str(), 0,
                               &OnDestResolutionIPv4, evkey);
            evdns_resolve_ipv6(cs->destname_.c_str(), 0,
                               &OnDestResolutionIPv6, evkey);
            cs->phase_ = PHASE_WAIT_DESTCONNECT;
            return;
          }
          case STATUS_INCOMPLETE: {
            return;
          }
          case STATUS_ABORT:
          default: {
            cs->Shut(WS_CLOSE_DESTINATION_ERROR,
                     "Incorrect destination specification in first frame");
            return;
          }
        }
      }
      case PHASE_WAIT_DESTCONNECT: {
        if (EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)) >=
            WebSocketProxy::kBufferLimit) {
          cs->Shut(WS_CLOSE_LIMIT_VIOLATION, "Read buffer overflow");
        }
        return;
      }
      case PHASE_OUTSIDE_FRAME: {
        switch (cs->ConsumeFrameHeader(EVBUFFER_INPUT(bev))) {
          case STATUS_OK: {
            if (cs->frame_bytes_remaining_ % 4) {
              // We expect base64 encoded data (encoded in 4-bytes chunks).
              cs->Shut(WS_CLOSE_UNACCEPTABLE_DATA,
                       "Frame payload size is not multiple of 4");
              return;
            }
            cs->phase_ = PHASE_INSIDE_FRAME_BASE64;
            // Process remaining data if any.
            break;
          }
          case STATUS_SKIP: {
            cs->phase_ = PHASE_INSIDE_FRAME_SKIP;
            // Process remaining data if any.
            break;
          }
          case STATUS_INCOMPLETE: {
            return;
          }
          case STATUS_ABORT:
          default: {
            cs->Shut(WS_CLOSE_PROTOCOL_ERROR, "Invalid frame header");
            return;
          }
        }
        break;
      }
      case PHASE_INSIDE_FRAME_BASE64:
      case PHASE_INSIDE_FRAME_SKIP: {
        switch (cs->ProcessFrameData(EVBUFFER_INPUT(bev))) {
          case STATUS_OK: {
            cs->phase_ = PHASE_OUTSIDE_FRAME;
            // Handle remaining data if any.
            break;
          }
          case STATUS_INCOMPLETE: {
            return;
          }
          case STATUS_ABORT:
          default: {
            cs->Shut(WS_CLOSE_UNACCEPTABLE_DATA, "Invalid frame data");
            return;
          }
        }
        break;
      }
      case PHASE_SHUT: {
        evbuffer_drain(EVBUFFER_INPUT(bev),
                       EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)));
        return;
      }
      case PHASE_DEFUNCT:
      default: {
        NOTREACHED();
        cs->master_->ZapConn(cs);
        return;
      }
    }
  }
}

// static
void Conn::OnPrimchanWrite(struct bufferevent* bev, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->primchan_.write_bev()) {
    NOTREACHED();
    return;
  }
  // Write callback is called when low watermark is reached, 0 by default.
  cs->primchan_.set_write_pending(false);
  if (cs->phase_ >= PHASE_SHUT) {
    cs->master_->ZapConn(cs);
    return;
  }
  if (cs->phase_ > PHASE_WAIT_DESTCONNECT)
    OnDestchanRead(cs->destchan_.read_bev(), evkey);
  if (cs->phase_ >= PHASE_SHUT)
    cs->primchan_.Zap();
}

// static
void Conn::OnPrimchanError(struct bufferevent* bev,
                           short what, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      (bev != cs->primchan_.read_bev() && bev != cs->primchan_.write_bev())) {
    return;
  }
  cs->primchan_.set_write_pending(false);
  if (cs->phase_ >= PHASE_SHUT)
    cs->master_->ZapConn(cs);
  else
    cs->Shut(WS_CLOSE_NORMAL, "Error reported on websocket channel");
}

// static
void Conn::OnDestResolutionIPv4(int result, char type,
                                int count, int ttl,
                                void* addr_list, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (cs == NULL)
    return;
  if (cs->phase_ != PHASE_WAIT_DESTCONNECT)
    return;
  if (result == DNS_ERR_NONE &&
      count >= 1 &&
      addr_list != NULL &&
      type == DNS_IPv4_A) {
    for (int i = 0; i < count; ++i) {
      struct sockaddr_in sa;
      memset(&sa, 0, sizeof(sa));
      sa.sin_family = AF_INET;
      sa.sin_port = htons(cs->destport_);
      DCHECK(sizeof(sa.sin_addr) == sizeof(struct in_addr));
      memcpy(&sa.sin_addr,
             static_cast<struct in_addr*>(addr_list) + i,
             sizeof(sa.sin_addr));
      if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa)))
        return;
    }
  }
  cs->destresolution_ipv4_failed_ = true;
  if (cs->destresolution_ipv4_failed_ && cs->destresolution_ipv6_failed_)
    cs->Shut(WS_CLOSE_RESOLUTION_FAILED, "DNS resolution failed");
}

// static
void Conn::OnDestResolutionIPv6(int result, char type,
                                int count, int ttl,
                                void* addr_list, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (cs == NULL)
    return;
  if (cs->phase_ != PHASE_WAIT_DESTCONNECT)
    return;
  if (result == DNS_ERR_NONE &&
      count >= 1 &&
      addr_list != NULL &&
      type == DNS_IPv6_AAAA) {
    for (int i = 0; i < count; ++i) {
      struct sockaddr_in6 sa;
      memset(&sa, 0, sizeof(sa));
      sa.sin6_family = AF_INET6;
      sa.sin6_port = htons(cs->destport_);
      DCHECK(sizeof(sa.sin6_addr) == sizeof(struct in6_addr));
      memcpy(&sa.sin6_addr,
             static_cast<struct in6_addr*>(addr_list) + i,
             sizeof(sa.sin6_addr));
      if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa)))
        return;
    }
  }
  cs->destresolution_ipv6_failed_ = true;
  if (cs->destresolution_ipv4_failed_ && cs->destresolution_ipv6_failed_)
    cs->Shut(WS_CLOSE_RESOLUTION_FAILED, "DNS resolution failed");
}

// static
void Conn::OnDestConnectTimeout(int, short, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (cs == NULL)
    return;
  if (cs->phase_ > PHASE_WAIT_DESTCONNECT)
    return;
  cs->Shut(WS_CLOSE_RESOLUTION_FAILED, "DNS resolution timeout");
}

// static
void Conn::OnDestchanRead(struct bufferevent* bev, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->destchan_.read_bev()) {
    NOTREACHED();
    return;
  }
  if (EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)) <= 0)
    return;
  if (cs->primchan_.write_bev() == NULL) {
    cs->master_->ZapConn(cs);
    return;
  }
  cs->master_->MarkConnImportance(cs, true);
  std::string out_bytes;
  base::Base64Encode(
      std::string(
          static_cast<const char*>(static_cast<void*>(
              EVBUFFER_DATA(EVBUFFER_INPUT(bev)))),
          EVBUFFER_LENGTH(EVBUFFER_INPUT(bev))),
      &out_bytes);
  evbuffer_drain(EVBUFFER_INPUT(bev), EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)));
  if (!cs->EmitFrame(&cs->primchan_, WS_OPCODE_TEXT,
                     out_bytes.c_str(), out_bytes.size())) {
    cs->Shut(WS_CLOSE_UNEXPECTED, "Failed to write websocket frame");
  }
}

// static
void Conn::OnDestchanWrite(struct bufferevent* bev, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->destchan_.write_bev()) {
    NOTREACHED();
    return;
  }
  // Write callback is called when low watermark is reached, 0 by default.
  cs->destchan_.set_write_pending(false);
  if (cs->phase_ == PHASE_WAIT_DESTCONNECT)
    cs->phase_ = PHASE_OUTSIDE_FRAME;
  if (cs->phase_ < PHASE_SHUT)
    OnPrimchanRead(cs->primchan_.read_bev(), evkey);
  else
    cs->destchan_.Zap();
}

// static
void Conn::OnDestchanError(struct bufferevent* bev,
                           short what, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      (bev != cs->destchan_.read_bev() && bev != cs->destchan_.write_bev())) {
    return;
  }
  cs->destchan_.set_write_pending(false);
  if (cs->phase_ >= PHASE_SHUT)
    cs->master_->ZapConn(cs);
  else
    cs->Shut(WS_CLOSE_DESTINATION_ERROR,
             "Failure reported on destination channel");
}

// static
Conn::EventKey Conn::last_evkey_ = 0;

// static
base::LazyInstance<Conn::EventKeyMap,
                   base::LeakyLazyInstanceTraits<Conn::EventKeyMap> >
    Conn::evkey_map_ = LAZY_INSTANCE_INITIALIZER;

}  // namespace

WebSocketProxy::WebSocketProxy(const std::vector<std::string>& allowed_origins)
    : impl_(new Serv(allowed_origins)) {
}

WebSocketProxy::~WebSocketProxy() {
  delete static_cast<Serv*>(impl_);
  impl_ = NULL;
}

void WebSocketProxy::Run() {
  static_cast<Serv*>(impl_)->Run();
}

void WebSocketProxy::Shutdown() {
  static_cast<Serv*>(impl_)->Shutdown();
}

void WebSocketProxy::OnNetworkChange() {
  static_cast<Serv*>(impl_)->OnNetworkChange();
}

}  // namespace chromeos
