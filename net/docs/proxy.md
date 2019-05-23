# Proxy support in Chrome

This document establishes basic proxy terminology, as well as describing
behaviors specific to Chrome.

## Proxy Server

A proxy server is an intermediary used for network requests. It can be
identified by the 3-tuple (scheme, host, port) where:

* scheme - protocol used to communicate with the proxy (ex: SOCKSv5, HTTPS).
* host - IP or hostname of the proxy server (ex: 192.168.0.1)
* port - TCP/UDP port number (ex: 443)

There are a variety of proxy server schemes supported by Chrome. When using an
explicit proxy in the browser, multiple layers of the network request are
impacted.

Difference between proxy server schemes include:

* Is communication to the proxy done over a secure channel?
* Is name resolution (ex: DNS) done client side, or proxy side?
* What authentication schemes to the proxy server are supported?
* What network traffic can be sent through the proxy?

Identifiers for proxy servers are often written as strings, using either the
PAC format (ex: `PROXY foo`) or Chrome's URI format (ex: `http://foo`).

When a proxy server's scheme is not stated, it's assumed to be HTTP in most
contexts.

This can lead to some confusion, particularly when discussing system proxy
settings. Major platform UIs have converged on the term "Secure proxy" to mean
the host:port for an (insecure) HTTP proxy to use for proxying https:// URLs.

So when someone refers to their "HTTPS proxy" be aware of this ambiguity. The
intended meaning could be either "an HTTP proxy for https:// URLs", or "a proxy
using the HTTPS scheme".

In this document when we say "an HTTPS proxy", we always mean "a proxy
that the browser speaks HTTPS to", and not "an (HTTP) proxy used to proxy
https:// URLs".

## Proxy resolution

Proxying in Chrome is done at the URL level.

When the browser is asked to fetch a URL, it needs to decide which IP endpoint
to send the request to. This can be either a proxy server, or the target host.

This is called proxy resolution. The input to proxy resolution is a URL, and
the output is an ordered list of proxy server options.

What proxies to use can be described using either:

* Manual proxy settings - proxy resolution is defined using a declarative set
  of rules. These rules are expressed as a mapping from URL scheme to proxy
  server(s), and a list of proxy bypass rules for when to go DIRECT instead of
  using the mapped proxy.

* PAC script - proxy resolution is defined using a JavaScript program, that is
  invoked whenever fetching a URL to get the list of proxy servers to use.

* Auto-detect - the WPAD protocol is used to probe the network (using DHCP/DNS)
  and possibly discover the URL of a PAC script.

## Proxy server schemes

Chrome supports the following proxy server schemes:

* DIRECT
* HTTP
* HTTPS
* SOCKSv4
* SOCKSv5
* QUIC

### DIRECT proxy scheme

* Default port: N/A (neither host nor port are applicable)
* Example identifier (PAC): `DIRECT`
* Example identifier (URI): `direct://`

This is a pseudo proxy scheme that indicates instead of using a proxy we are
sending the request directly to the target server.

It is imprecise to call this a "proxy server", but it is a convenient abstraction.

### HTTP proxy scheme

* Default port: 80
* Example identifier (PAC): `PROXY proxy:8080`, `proxy` (non-standard; don't use)
* Example identifiers (URI): `http://proxy:8080`, `proxy:8080` (can omit scheme)

Generally when one refers to a "proxy server" or "web proxy", they are talking
about an HTTP proxy.

When using an HTTP proxy in Chrome, name resolution is always deferred to the
proxy. HTTP proxies can proxy `http://`, `https://`, `ws://` and `wss://` URLs.
(Chrome's FTP support is deprecated, and HTTP proxies cannot proxy `ftp://` anymore)

Communication to HTTP proxy servers is insecure, meaning proxied `http://`
requests are sent in the clear. When proxying `https://` requests through an
HTTP proxy, the TLS exchange is forwarded through the proxy using the `CONNECT`
method, so end-to-end encryption is not broken. However when establishing the
tunnel, the hostname of the target URL is sent to the proxy server in the
clear.

HTTP proxies in Chrome support the same HTTP authentiation schemes as for
target servers: Basic, Digest, Negotiate/NTLM.

### HTTPS proxy scheme

* Default port: 443
* Example identifier (PAC): `HTTPS proxy:8080`
* Example identifier (URI): `https://proxy:8080`

This works exactly like an HTTP proxy, except the communication to the proxy
server is protected by TLS. Hence `http://` requests, and hostnames for
`https://` requests are not sent in the clear as with HTTP proxies.

In addition to HTTP authentication methods, one can also use client
certificates to authenticate to HTTPS proxies.

### SOCKSv4 proxy scheme

* Default port: 1080
* Example identifiers (PAC): `SOCKS4 proxy:8080`, `SOCKS proxy:8080`
* Example identifier (URI): `socks4://proxy:8080`

SOCKSv4 is a simple transport layer proxy that wraps a TCP socket. Its use
is transparent to the rest of the protocol stack; after an initial
handshake when connecting the TCP socket (to the proxy), the rest of the
loading stack is unchanged.

No proxy authentication methods are supported for SOCKSv4.

When using a SOCKSv4 proxy, name resolution for target hosts is always done
client side, and moreover must resolve to an IPv4 address (SOCKSv4 encodes
target address as 4 octets, so IPv6 targets are not possible).

There are extensions to SOCKSv4 that allow for proxy side name resolution, and
IPv6, namely SOCKSv4a. However Chrome does not allow configuring, or falling
back to v4a.

A better alternative is to just use the newer version of the protocol, SOCKSv5
(which is still 20+ years old).

### SOCKSv5 proxy scheme

* Default port: 1080
* Example identifier (PAC): `SOCKS5 proxy:8080`
* Example identifiers (URI): `socks://proxy:8080`, `socks5://proxy:8080`

[SOCKSv5](https://tools.ietf.org/html/rfc1928) is a transport layer proxy that
wraps a TCP socket, and allows for name resolution to be deferred to the proxy.

In Chrome when a proxy's scheme is set to SOCKSv5, name resolution is always
done proxy side (even though the protocol allows for client side as well). In
Firefox client side vs proxy side name resolution can be configured with
`network.proxy.socks_remote_dns`; Chrome has no equivalent option and will
always use proxy side resolution.

No authentication methods are supported for SOCKSv5 in Chrome (although some do
exist for the protocol).

A handy way to create a SOCKSv5 proxy is with `ssh -D`, which can be used to
tunnel web traffic to a remote host over SSH.

In Chrome SOCKSv5 is only used to proxy TCP-based URL requests. It cannot be
used to relay UDP traffic.

### QUIC proxy scheme

* Default (UDP) port: 443
* Example identifier (PAC): `QUIC proxy:8080`
* Example identifier (URI): `quic://proxy:8080`

TODO

## Implicit bypass rules

Requests to certain hosts will not be sent through a proxy, and will instead be
sent directly.

We call these the _implicit bypass rules_. The implicit bypass rules match URLs
whose host portion is either a localhost name or a link-local IP literal.
Essentially it matches:

```
localhost
*.localhost
[::1]
127.0.0.1/8
169.254/16
[FE80::]/10
```

The complete rules are slightly more complicated. For instance on
Windows we will also recognize `loopback`, and there is special casing of
`localhost6` and `localhost6.localdomain6` in Chrome's localhost matching.

This concept of implicit proxy bypass rules is consistent with the
platform-level proxy support on Windows and macOS (albeit with some differences
due to their implementation quirks - see compatibility notes in
`net::ProxyBypassRules::MatchesImplicitRules`)

Why apply implicit proxy bypass rules in the first place? Certainly there are
considerations around ergonomics and user expectation, but the bigger problem
is security. Since the web platform treats `localhost` as a secure origin, the
ability to proxy it grants extra powers. This is [especially
problematic](https://bugs.chromium.org/p/chromium/issues/detail?id=899126) when
proxy settings are externally controllable, as when using PAC scripts.

Historical support in Chrome:

* Prior to M71 there were no implicit proxy bypass rules (except if using
  `--winhttp-proxy-resolver`)
* In M71 Chrome applied implicit proxy bypass rules to PAC scripts
* In M72 Chrome generalized the implicit proxy bypass rules to manually
  configured proxies

## Overriding the implicit bypass rules

If you want traffic to `localhost` to be sent through a proxy despite the
security concerns, it can be done by adding the special proxy bypass rule
`<-loopback>`. This has the effect of _subtracting_ the implicit rules.

For instance, launch Chrome with the command line flag:

```
--proxy-bypass-list="<-loopback>"
```

Note that there currently is no mechanism to disable the implicit proxy bypass
rules when using a PAC script. Proxy bypass lists only apply to manual
settings, so the technique above cannot be used to let PAC scripts decide the
proxy for localhost URLs.

## Evaluating proxy lists (proxy fallback)

Proxy resolution results in a _list_ of proxy servers to use for a given
request, not just a single proxy server.

For instance, consider this PAC script:

```
function FindProxyForURL(url, host) {
    if (host == "www.example.com") {
        return "PROXY proxy1; HTTPS proxy2; SOCKS5 proxy3";
    }
    return "DIRECT";
}

```

What proxy will Chrome use for connections to `www.example.com`, given that
we have a choice of 3 separate proxies, each of different type?

Initially, Chrome will try the proxies in order. This means first attempting the
request through the HTTP WebProxy `proxy1`. If that "fails", the request is
next attempted through the HTTPS proxy `proxy2`. Lastly if that fails, the
request is attempted through the SOCKSv5 proxy `proxy3`.

This process is referred to as _proxy fallback_. What constitutes a
"failure" is described later.

Proxy fallback is stateful. The actual order of proxy attempts made be Chrome
is influenced by the past responsiveness of proxy servers.

Let's say we request `http://www.example.com/`. Per the PAC script this
resolves to:

```
"PROXY proxy1; HTTPS proxy2; SOCKS5 proxy3"
```

Chrome will first attempt to issue the request through these proxies in the
left-to-right order (`proxy1`, `proxy2`, `proxy3`).

Let's say that the attempt through `proxy1` fails, but then the attempt through
`proxy2` succeeds. Chrome will mark `proxy1` as _bad_ for the next 5 minutes.
Being marked as _bad_ means that `proxy1` is de-prioritized with respect to
other proxies options (including DIRECT) that are not marked as bad.

That means the next time `http://www.example.com/` is requested, the effective
order for proxies to attempt will be:

```
HTTPS proxy2; SOCKS5 proxy3; "PROXY proxy1"
```

Conceptually, _bad_ proxies are moved to the end of the list, rather than being
removed from consideration all together.

What constitutes a "failure" when it comes to triggering proxy fallback depends
on the proxy type. Generally speaking, only connection level failures
are deemed eligible for proxy fallback. This includes:

* Failure resolving the proxy server's DNS
* Failure connecting a TCP socket to the proxy server

(There are some caveats for how HTTPS and QUIC proxies count failures for
fallback)

Prior to M67, Chrome would consider failures establishing a
CONNECT tunnel as an error eligible for proxy fallback. This policy [resulted
in problems](https://bugs.chromium.org/p/chromium/issues/detail?id=680837) for
deployments whose HTTP proxies intentionally failed certain https:// requests,
since that necessitates inducing a failure during the CONNECT tunnel
establishment. The problem would occur when a working proxy fallback option
like DIRECT was given, since the failing proxy would then be marked as bad.

Currently there are no options to configure proxy fallback (including disabling
the caching of bad proxies). Future versions of Chrome may [remove caching
of bad proxies](https://bugs.chromium.org/p/chromium/issues/detail?id=936130)
to make fallback predictable.

To investigate issues relating to proxy fallback, one can [collect a NetLog
dump using
chrome://net-export/](https://dev.chromium.org/for-testers/providing-network-details).
These logs can then be loaded with the [NetLog
viewer](https://netlog-viewer.appspot.com/).

There are a few things of interest in the logs:

* The "Proxy" tab will show which proxies (if any) were marked as bad at the
  time the capture ended.
* The "Events" tab notes what the resolved proxy list was, and what the
  re-ordered proxy list was after taking into account bad proxies.
* The "Events" tab notes when a proxy is marked as bad and why (provided the
  event occurred while capturing was enabled).

When debugging issues with bad proxies, it is also useful to reset Chrome's
cache of bad proxies. This can be done by clicking the "Clear bad proxies"
button on
[chrome://net-internals/#proxy](chrome://net-internals/#proxy). Note the UI
will not give feedback that the bad proxies were cleared, however capturing a
new NetLog dump can confirm it was cleared.

## Arguments passed to `FindProxyForURL(url, host)` in PAC scripts

PAC scripts in Chrome are expected to define a JavaScript function
`FindProxyForURL`.

The historical signature for this function is:

```
function FindProxyForURL(url, host) {
  ...
}
```

Scripts can expect to be called with string arguments `url` and `host` such
that:

* `url` is a *sanitized* version of the request's URL
* `host` is the unbracketed host portion of the origin.

Sanitization of the URL means that the path, query, fragment, and identity
portions of the URL are stripped. Effectively `url` will be
limited to a `scheme://host:port/` style URL

Examples of how `FindProxyForURL()` will be called:

```
// Actual URL:   https://www.google.com/Foo
FindProxyForURL('https://www.google.com/', 'www.google.com')

// Actual URL:   https://[dead::beef]/foo?bar
FindProxyForURL('https://[dead::beef]/', 'dead::beef')

// Actual URL:   https://www.example.com:8080#search
FindProxyForURL('https://www.example.com:8080/', 'example.com')

// Actual URL:   https://username:password@www.example.com
FindProxyForURL('https://www.example.com/', 'example.com')
```

Stripping the path and query from the `url` is a departure from the original
Netscape implementation of PAC. It was introduced in Chrome 52 for [security
reasons](https://bugs.chromium.org/p/chromium/issues/detail?id=593759).

There is currently no option to turn off sanitization of URLs passed to PAC
scripts (removed in Chrome 75).

The sanitization of http:// URLs currently has a different policy, and does not
strip query and path portions of the URL. That said, users are advised not to
depend on reading the query/path portion of any URL
type, since future versions of Chrome may [deprecate that
capability](https://bugs.chromium.org/p/chromium/issues/detail?id=882536) in
favor of a consistent policy.

## Resolving client's IP address within a PAC script using `myIpAddress()`

PAC scripts can invoke `myIpAddress()` to obtain the client's IP address. This
function returns a single IP literal, or `"127.0.0.1"` on failure.

`myIpAddress()` is fundamentally broken for multi-homed hosts.

Consider what happens when a machine has multiple network interfaces, each with
its own IP address. Answering "what is my IP address" depends on what interface
the request is sent out on. Which in turn depends on what the destination IP
is. Which in turn depends on the result of proxy resolution + fallback, which
is what we are currently blocked in!

Chrome's algorithm uses these ordered steps to find an IP address
(short-circuiting when a candidate is found).

1. Select the IP of an interface that can route to public Internet:
    * Probe for route to `8.8.8.8`.
    * Probe for route to `2001:4860:4860::8888`.
2. Select an IP by doing a DNS resolve of the machine's hostname:
    * Select the first IPv4 result if there is one.
    * Select the first IP result if there is one.
3. Select the IP of an interface that can route to private IP space:
    * Probe for route to `10.0.0.0`.
    * Probe for route to `172.16.0.0`.
    * Probe for route to `192.168.0.0`.
    * Probe for route to `FC00::`.

When searching for candidate IP addresses, link-local and loopback addresses
are skipped over. Link-local or loopback address will only be returned as a
last resort when no other IP address was found by following these steps.

This sequence of steps explicitly favors IPv4 over IPv6 results.

*Historical note*: Prior to M72, Chrome's implementation of `myIpAddress()` was
effectively just `getaddrinfo(gethostname)`. This is now step 2 of the heuristic.

### What about `var pacUseMultihomedDNS`?

In Firefox, if you define a global named `pacUseMultihomedDNS` in your PAC
script, it causes `myIpAddress()` to report the IP address of the interface
that would (likely) have been used had we connected to it DIRECT.

In particular, it will do a DNS resolution of the target host (the hostname of
the URL that the proxy resolution is being done for), and then
connect a datagram socket to get the source address.

Chrome does not recognize the `pacUseMultihomedDNS` global as having special
meaning. A PAC script is free to define such a global, and it won't have
side-effects. Chrome has no APIs or settings to change `myIpAddress()`'s
algorithm.

## Resolving client's IP address within a PAC script using `myIpAddressEx()`

Chrome supports the [Microsoft PAC
extension](https://docs.microsoft.com/en-us/windows/desktop/winhttp/myipaddressex)
`myIpAddressEx()`.

This is like `myIpAddress()`, but instead of returning a single IP address, it
can return multiple IP addresses. It returns a string containing a semi-colon
separated list of addresses. On failure it returns an empty string to indicate
no results (whereas `myIpAddress()` returns `127.0.0.1`).

There are some differences with Chrome's implementation:

* In Chrome the function is unconditionally defined, whereas in Internet
  Explorer one must have used the `FindProxyForURLEx` entrypoint.
* Chrome does not enumerate all of the host's network interfaces
* Chrome does not return link-local or loopback addresses (except if no other
  addresses were found).

The algorithm that Chrome uses is nearly identical to that of `myIpAddress()`
described earlier. The main difference is that we don't short-circuit
after finding the first candidate IP, so multiple IPs may be returned.

1. Select all the IPs of interfaces that can route to public Internet:
    * Probe for route to `8.8.8.8`.
    * Probe for route to `2001:4860:4860::8888`.
    * If any IPs were found, return them, and finish.
2. Select an IP by doing a DNS resolve of the machine's hostname:
    * If any IPs were found, return them, and finish.
3. Select the IP of an interface that can route to private IP space:
    * Probe for route to `10.0.0.0`.
    * Probe for route to `172.16.0.0`.
    * Probe for route to `192.168.0.0`.
    * Probe for route to `FC00::`.
    * If any IPs were found, return them, and finish.

Note that short-circuiting happens whenever steps 1-3 find a candidate IP. So
for example if at least one IP address was discovered by checking routes to
public Internet, only those IPs will be returned, and steps 2-3 will not run.
