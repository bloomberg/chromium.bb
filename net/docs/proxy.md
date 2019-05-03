# Proxy support

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

## Arguments are passed to `FindProxyForURL(url, host)` in PAC scripts

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
