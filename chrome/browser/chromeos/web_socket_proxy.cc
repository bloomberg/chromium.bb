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
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/internal_auth.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/browser_thread.h"
#include "content/common/content_notification_types.h"
#include "content/common/notification_service.h"
#include "content/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "third_party/libevent/evdns.h"
#include "third_party/libevent/event.h"

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

// Returns true on success.
bool FetchDecimalDigits(const std::string& s, uint32* result) {
  *result = 0;
  bool got_something = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (IsAsciiDigit(s[i])) {
      got_something = true;
      if (*result > std::numeric_limits<uint32>::max() / 10)
        return false;
      *result *= 10;
      int digit = s[i] - '0';
      if (*result > std::numeric_limits<uint32>::max() - digit)
        return false;
      *result += digit;
    }
  }
  return got_something;
}

// Parses "passport:hostname:port:" string.  Returns true on success.
bool FetchPassportNamePort(
    uint8* begin, uint8* end,
    std::string* passport, std::string* name, uint32* port) {
  std::string input(begin, end);
  if (input[input.size() - 1] != ':')
    return false;
  input.resize(input.size() - 1);

  size_t pos = input.find_last_of(':');
  if (pos == std::string::npos)
    return false;
  std::string port_str(input, pos + 1);
  if (port_str.empty())
    return false;
  const char kAsciiDigits[] = "0123456789";
  COMPILE_ASSERT(sizeof(kAsciiDigits) == 10 + 1, mess_with_digits);
  if (port_str.find_first_not_of(kAsciiDigits) != std::string::npos)
    return false;
  if (!FetchDecimalDigits(port_str, port) ||
      *port <= 0 ||
      *port >= (1 << 16)) {
    return false;
  }
  input.resize(pos);

  pos = input.find_first_of(':');
  if (pos == std::string::npos)
    return false;
  passport->assign(input, 0, pos);
  name->assign(input, pos + 1, std::string::npos);
  return !name->empty();
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

void SendNotification() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_WEB_SOCKET_PROXY_STARTED,
      NotificationService::AllSources(), NotificationService::NoDetails());
}

class Conn;

// Websocket to TCP proxy server.
class Serv {
 public:
  Serv(const std::vector<std::string>& allowed_origins,
       struct sockaddr* addr, int addr_len);
  ~Serv();

  // Do not call it twice.
  void Run();

  // Terminates running server (should be called on a different thread).
  void Shutdown();

  void ZapConn(Conn*);
  void MarkConnImportance(Conn*, bool important);
  Conn* GetFreshConn();
  bool IsConnSane(Conn*);
  bool IsOriginAllowed(const std::string& origin);
  void CloseAll();

  static void OnConnect(int listening_sock, short event, void*);
  static void OnShutdownRequest(int fd, short event, void*);

  struct event_base* evbase() { return evbase_; }

  // Checked against value of Origin field specified
  // in a client websocket handshake.
  std::vector<std::string> allowed_origins_;

  // Address to listen incoming websocket connections.
  struct sockaddr* addr_;
  int addr_len_;

  // Libevent base.
  struct event_base* evbase_;

  // Socket to listen incoming websocket connections.
  int listening_sock_;

  // Event on this descriptor triggers server shutdown.
  int shutdown_descriptor_[2];

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
  scoped_ptr<struct event> shutdown_event_;

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

  // Channel structure (either proxy<->javascript or proxy<->destination).
  class Chan {
   public:
    explicit Chan(Conn* master)
        : master_(master), sock_(-1), bev_(NULL), write_pending_(false) {
    }

    ~Chan() {
      Zap();
    }

    // Returns true on success.
    bool Write(const void* data, size_t size) {
      if (bev_ == NULL || sock_ < 0)
        return false;
      write_pending_ = true;
      return (0 == bufferevent_write(bev_, data, size));
    }

    void Zap() {
      if (bev_) {
        bufferevent_disable(bev_, EV_READ | EV_WRITE);
        bufferevent_free(bev_);
        bev_ = NULL;
      }
      if (sock_ >= 0) {
        shutdown(sock_, SHUT_RDWR);
        close(sock_);
        sock_ = -1;
      }
      write_pending_ = false;
      master_->ConsiderSuicide();
    }

    void Shut() {
      if (!write_pending_)
        Zap();
    }

    int& sock() { return sock_; }
    bool& write_pending() { return write_pending_; }
    struct bufferevent*& bev() { return bev_; }

   private:
    Conn* master_;
    int sock_;  // UNIX descriptor.
    struct bufferevent* bev_;
    bool write_pending_;  // Whether write buffer is not flushed yet.
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

  // Hostname and port of destination socket.
  // Websocket client supplies them in first data frame (destframe).
  std::string destname_;
  uint32 destport_;

  // We try to DNS resolve hostname in both IPv4 and IPv6 domains.
  // Track resolution failures here.
  bool destresolution_ipv4_failed_;
  bool destresolution_ipv6_failed_;

  // Used to schedule a timeout for initial phase of connection.
  scoped_ptr<struct event> destconnect_timeout_event_;

  static EventKeyMap evkey_map_;
  static EventKey last_evkey_;

  DISALLOW_COPY_AND_ASSIGN(Conn);
};

Serv::Serv(
    const std::vector<std::string>& allowed_origins,
    struct sockaddr* addr, int addr_len)
    : allowed_origins_(allowed_origins),
      addr_(addr),
      addr_len_(addr_len),
      evbase_(NULL),
      listening_sock_(-1),
      shutdown_requested_(false) {
  std::sort(allowed_origins_.begin(), allowed_origins_.end());
  shutdown_descriptor_[0] = -1;
  shutdown_descriptor_[1] = -1;
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

  if (pipe(shutdown_descriptor_)) {
    LOG(ERROR) << "WebSocketProxy: Failed to create shutdown pipe";
    return;
  }

  listening_sock_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listening_sock_ < 0) {
    LOG(ERROR) << "WebSocketProxy: Failed to create socket";
    return;
  }
  if (bind(listening_sock_, addr_, addr_len_)) {
    LOG(ERROR) << "WebSocketProxy: Failed to bind server socket";
    return;
  }
  if (listen(listening_sock_, 12)) {
    LOG(ERROR) << "WebSocketProxy: Failed to listen server socket";
    return;
  }
  {
    int on = 1;
    setsockopt(listening_sock_, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
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

  shutdown_event_.reset(new struct event);
  event_set(shutdown_event_.get(), shutdown_descriptor_[0], EV_READ,
            &OnShutdownRequest, this);
  event_base_set(evbase_, shutdown_event_.get());
  if (event_add(shutdown_event_.get(), NULL)) {
    LOG(ERROR) << "WebSocketProxy: Failed to add shutdown event";
    return;
  }

  if (evdns_init()) {
    LOG(ERROR) << "WebSocketProxy: Failed to initialize evDNS";
    return;
  }
  if (!IgnoreSigPipe()) {
    LOG(ERROR) << "WebSocketProxy: Failed to ignore SIGPIPE";
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&SendNotification));

  LOG(INFO) << "WebSocketProxy: Starting event dispatch loop.";
  event_base_dispatch(evbase_);
  if (shutdown_requested_)
    LOG(INFO) << "WebSocketProxy: Event dispatch loop terminated upon request";
  else
    LOG(ERROR) << "WebSocketProxy: Event dispatch loop terminated unexpectedly";
  CloseAll();
}

void Serv::Shutdown() {
  if (1 != write(shutdown_descriptor_[1], ".", 1))
    NOTREACHED();
}

void Serv::CloseAll() {
  while (!conn_pool_.empty())
    ZapConn(conn_pool_.back());
  if (listening_sock_ >= 0) {
    shutdown(listening_sock_, SHUT_RDWR);
    close(listening_sock_);
  }
  for (int i = 0; i < 2; ++i) {
    if (shutdown_descriptor_[i] >= 0) {
      shutdown_descriptor_[i] = -1;
      close(shutdown_descriptor_[i]);
    }
  }
  if (shutdown_event_.get()) {
    event_del(shutdown_event_.get());
    shutdown_event_.reset();
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
  cs->primchan().sock() = accept(listening_sock, NULL, NULL);
  if (cs->primchan().sock() < 0
      || !SetNonBlock(cs->primchan().sock())) {
    // Read readiness was triggered on listening socket
    // yet we failed to accept a connection; definitely weird.
    NOTREACHED();
    self->ZapConn(cs);
    return;
  }

  cs->primchan().bev() = bufferevent_new(
      cs->primchan().sock(),
      &Conn::OnPrimchanRead, &Conn::OnPrimchanWrite, &Conn::OnPrimchanError,
      cs->evkey());
  if (cs->primchan().bev() == NULL) {
    self->ZapConn(cs);
    return;
  }
  bufferevent_base_set(self->evbase_, cs->primchan().bev());
  bufferevent_setwatermark(
      cs->primchan().bev(), EV_READ, 0, WebSocketProxy::kReadBufferLimit);
  if (bufferevent_enable(cs->primchan().bev(), EV_READ | EV_WRITE)) {
    self->ZapConn(cs);
    return;
  }
}

// static
void Serv::OnShutdownRequest(int fd, short event, void* ctx) {
  Serv* self = static_cast<Serv*>(ctx);
  self->shutdown_requested_ = true;
  event_base_loopbreak(self->evbase_);
}

Conn::Conn(Serv* master)
    : master_(master),
      phase_(PHASE_WAIT_HANDSHAKE),
      frame_bytes_remaining_(0),
      frame_mask_index_(0),
      primchan_(this),
      destchan_(this),
      destresolution_ipv4_failed_(false),
      destresolution_ipv6_failed_(false) {
  while (evkey_map_.find(last_evkey_) != evkey_map_.end()) {
    evkey_ = last_evkey_ =
        reinterpret_cast<EventKey>(reinterpret_cast<size_t>(last_evkey_) + 1);
  }
  evkey_map_[evkey_] = this;
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
  if (evkey_map_[evkey_] == this)
    evkey_map_.erase(evkey_);
  else
    NOTREACHED();
}

Conn* Conn::Get(EventKey evkey) {
  EventKeyMap::iterator it = evkey_map_.find(evkey);
  if (it == evkey_map_.end())
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

  static const uint8 kGetMagic[] = "GET " kProxyPath " ";
  static const uint8 kKeyValueDelimiter[] = ": ";

  if (buf_size <= 0)
    return STATUS_INCOMPLETE;
  if (!buf)
    return STATUS_ABORT;
  if (!std::equal(buf, buf + std::min(buf_size, strlen(kGetMagic)),
                  kGetMagic)) {
    // Data head does not match what is expected.
    return STATUS_ABORT;
  }

  if (buf_size >= WebSocketProxy::kHeaderLimit)
    return STATUS_ABORT;
  uint8* buf_end = buf + buf_size;
  uint8* term_pos = std::search(buf, buf_end, kCRLFCRLF,
                                  kCRLFCRLF + strlen(kCRLFCRLF));
  term_pos += strlen(kCRLFCRLF);
  // First line is "GET /tcpproxy" line, so we skip it.
  uint8* pos = std::search(buf, term_pos, kCRLF, kCRLF + strlen(kCRLF));
  if (pos == term_pos)
    return STATUS_ABORT;
  for (;;) {
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
  // Here we check origin.  This check may seem redundant because we verify
  // passport token later.  However the earlier we can reject connection the
  // better.  We receive origin field in websocket header way before receiving
  // passport string.
  if (!master_->IsOriginAllowed(origin.spec()))
    return STATUS_ABORT;

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
  if (!FetchPassportNamePort(buf, buf + frame_bytes_remaining_,
                             &passport, &destname_, &destport_)) {
    return STATUS_ABORT;
  }
  std::map<std::string, std::string> map;
  map["hostname"] = destname_;
  map["port"] = base::IntToString(destport_);
  map["extension_id"] = FetchExtensionIdFromOrigin(GetOrigin());
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
    case 1:  // Text frame.
      break;
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
      DCHECK(destchan_.bev() != NULL);
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
  if (destchan_.sock() >= 0 || destchan_.bev() != NULL)
    return false;
  destchan_.sock() = socket(addr->sa_family, SOCK_STREAM, 0);
  if (destchan_.sock() < 0)
    return false;
  if (!SetNonBlock(destchan_.sock()))
    return false;
  if (connect(destchan_.sock(), addr, addrlen)) {
    if (errno != EINPROGRESS)
      return false;
  }
  destchan_.bev() = bufferevent_new(
      destchan_.sock(),
      &OnDestchanRead, &OnDestchanWrite, &OnDestchanError,
      evkey_);
  if (destchan_.bev() == NULL)
    return false;
  if (bufferevent_base_set(master_->evbase(), destchan_.bev()))
    return false;
  bufferevent_setwatermark(
      destchan_.bev(), EV_READ, 0, WebSocketProxy::kReadBufferLimit);
  return !bufferevent_enable(destchan_.bev(), EV_READ | EV_WRITE);
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
      bev != cs->primchan_.bev()) {
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
        return;
      }
      case PHASE_WAIT_DESTFRAME: {
        switch (cs->ConsumeDestframe(EVBUFFER_INPUT(bev))) {
          case STATUS_OK: {
            {
              struct sockaddr_in sa;
              memset(&sa, 0, sizeof(sa));
              sa.sin_port = htons(cs->destport_);
              if (inet_pton(sa.sin_family = AF_INET,
                            cs->destname_.c_str(),
                            &sa.sin_addr) == 1) {
                // valid IPv4 address supplied.
                if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa))) {
                  cs->phase_ = PHASE_WAIT_DESTCONNECT;
                  return;
                }
              }
            }
            {
              if (cs->destname_.size() >= 2 &&
                  cs->destname_[0] == '[' &&
                  cs->destname_[cs->destname_.size() - 1] == ']') {
                // Literal IPv6 address in brackets.
                cs->destname_ =
                    cs->destname_.substr(1, cs->destname_.size() - 2);
              }
              struct sockaddr_in6 sa;
              memset(&sa, 0, sizeof(sa));
              sa.sin6_port = htons(cs->destport_);
              if (inet_pton(sa.sin6_family = AF_INET6,
                            cs->destname_.c_str(),
                            &sa.sin6_addr) == 1) {
                // valid IPv6 address supplied.
                if (cs->TryConnectDest((struct sockaddr*)&sa, sizeof(sa))) {
                  cs->phase_ = PHASE_WAIT_DESTCONNECT;
                  return;
                }
              }
            }
            // Try to asynchronously perform DNS resolution.
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
            WebSocketProxy::kReadBufferLimit) {
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
      bev != cs->primchan_.bev()) {
    NOTREACHED();
    return;
  }
  cs->primchan_.write_pending() = false;
  if (cs->phase_ >= PHASE_SHUT) {
    cs->master_->ZapConn(cs);
    return;
  }
  if (cs->phase_ > PHASE_WAIT_DESTCONNECT)
    OnDestchanRead(cs->destchan_.bev(), evkey);
  if (cs->phase_ >= PHASE_SHUT)
    cs->primchan_.Zap();
}

// static
void Conn::OnPrimchanError(struct bufferevent* bev,
                           short what, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->primchan_.bev()) {
    return;
  }
  cs->primchan_.write_pending() = false;
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
      bev != cs->destchan_.bev()) {
    NOTREACHED();
    return;
  }
  if (EVBUFFER_LENGTH(EVBUFFER_INPUT(bev)) <= 0)
    return;
  if (cs->primchan_.bev() == NULL) {
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
      bev != cs->destchan_.bev()) {
    NOTREACHED();
    return;
  }
  cs->destchan_.write_pending() = false;
  if (cs->phase_ == PHASE_WAIT_DESTCONNECT)
    cs->phase_ = PHASE_OUTSIDE_FRAME;
  if (cs->phase_ < PHASE_SHUT)
    OnPrimchanRead(cs->primchan_.bev(), evkey);
  else
    cs->destchan_.Zap();
}

// static
void Conn::OnDestchanError(struct bufferevent* bev,
                           short what, EventKey evkey) {
  Conn* cs = Conn::Get(evkey);
  if (bev == NULL ||
      cs == NULL ||
      bev != cs->destchan_.bev()) {
    return;
  }
  cs->destchan_.write_pending() = false;
  if (cs->phase_ >= PHASE_SHUT)
    cs->master_->ZapConn(cs);
  else
    cs->Shut(WS_CLOSE_DESTINATION_ERROR,
             "Failure reported on destination channel");
}

Conn::EventKey Conn::last_evkey_ = 0;
Conn::EventKeyMap Conn::evkey_map_;

}  // namespace

WebSocketProxy::WebSocketProxy(
    const std::vector<std::string>& allowed_origins,
    struct sockaddr* addr, int addr_len)
    : impl_(new Serv(allowed_origins, addr, addr_len)) {
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

}  // namespace chromeos

