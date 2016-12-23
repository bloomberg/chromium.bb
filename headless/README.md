# Headless Chromium

Headless Chromium allows running Chromium in a headless/server environment.
Expected use cases include loading web pages, extracting metadata (e.g., the
DOM) and generating bitmaps from page contents -- using all the modern web
platform features provided by Chromium and Blink.

There are two ways to use Headless Chromium:

## Usage via the DevTools remote debugging protocol

1. Start a normal Chrome binary with the `--headless` command line flag
(Linux-only for now):

```
$ chrome --headless --remote-debugging-port=9222 https://chromium.org
```

Currently you'll also need to use `--disable-gpu` to avoid an error from a
missing Mesa library.

2. Navigate to `http://localhost:9222` in another browser to open the
[DevTools](https://developer.chrome.com/devtools) interface or use a tool such
as [Selenium](http://www.seleniumhq.org/) to drive the headless browser.

## Usage as a C++ library

Headless Chromium can be built as a library for embedding into a C++
application. This approach is otherwise similar to controlling the browser over
a DevTools connection, but it provides more customization points, e.g., for
networking and [mojo services](https://docs.google.com/document/d/1Fr6_DJH6OK9rG3-ibMvRPTNnHsAXPk0VzxxiuJDSK3M/edit#heading=h.qh0udvlk963d).

Headless Shell is a sample application which demonstrates the use of the
headless C++ API. To run it, first initialize a headless build configuration:

```
$ mkdir -p out/Debug
$ echo 'import("//build/args/headless.gn")' > out/Debug/args.gn
$ gn gen out/Debug
```

Then build the shell:

```
$ ninja -C out/Debug headless_shell
```

After the build completes, Headless Shell can be run with the following command:

```
$ out/Debug/headless_shell https://www.google.com
```

To attach a [DevTools](https://developer.chrome.com/devtools) debugger to the
shell, start it with an argument specifying the debugging port:

```
$ out/Debug/headless_shell --remote-debugging-port=9222 https://youtube.com
```

Then navigate to `http://localhost:9222` with your browser.

## Embedder API

The embedder API allows developers to integrate the headless library into their
application. The API provides default implementations for low level adaptation
points such as networking and the run loop.

The main embedder API classes are:

- `HeadlessBrowser::Options::Builder` - Defines the embedding options, e.g.:
  - `SetMessagePump` - Replaces the default base message pump. See
    `base::MessagePump`.
  - `SetProxyServer` - Configures an HTTP/HTTPS proxy server to be used for
    accessing the network.

## Client/DevTools API

The headless client API is used to drive the browser and interact with loaded
web pages. Its main classes are:

- `HeadlessBrowser` - Represents the global headless browser instance.
- `HeadlessWebContents` - Represents a single "tab" within the browser.
- `HeadlessDevToolsClient` - Provides a C++ interface for inspecting and
  controlling a tab. The API functions corresponds to [DevTools commands](https://developer.chrome.com/devtools/docs/debugger-protocol).
  See the [client API documentation](https://docs.google.com/document/d/1rlqcp8nk-ZQvldNJWdbaMbwfDbJoOXvahPCDoPGOwhQ/edit#)
  for more information.

## Resources and Documentation

Mailing list: [headless-dev@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/headless-dev)
Bug tracker: [Proj=Headless](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Proj%3DHeadless)

* [Runtime headless mode for Chrome](https://docs.google.com/document/d/1aIJUzQr3eougZQp90bp4mqGr5gY6hdUice8UPa-Ys90/edit#)
* [Virtual Time in Blink](https://docs.google.com/document/d/1y9kdt_zezt7pbey6uzvt1dgklwc1ob_vy4nzo1zbqmo/edit#heading=h.tn3gd1y9ifml)
* [Headless Chrome architecture](https://docs.google.com/document/d/11zIkKkLBocofGgoTeeyibB2TZ_k7nR78v7kNelCatUE/edit)
* [Headless Chrome C++ DevTools API](https://docs.google.com/document/d/1rlqcp8nk-ZQvldNJWdbaMbwfDbJoOXvahPCDoPGOwhQ/edit#heading=h.ng2bxb15li9a)
* [Session isolation in Headless Chrome](https://docs.google.com/document/d/1XAKvrxtSEoe65vNghSWC5S3kJ--z2Zpt2UWW1Fi8GiM/edit)
* [Headless Chrome mojo service](https://docs.google.com/document/d/1Fr6_DJH6OK9rG3-ibMvRPTNnHsAXPk0VzxxiuJDSK3M/edit#heading=h.qh0udvlk963d)
* [Controlling BeginFrame through DevTools](https://docs.google.com/document/d/1LVMYDkfjrrX9PNkrD8pJH5-Np_XUTQHIuJ8IEOirQH4/edit?ts=57d96dbd#heading=h.ndv831lc9uf0)
* [Viewport bounds and scale for screenshots](https://docs.google.com/document/d/1VTcYz4q_x0f1O5IVrvRX4u1DVd_K34IVUl1VULLTCWw/edit#heading=h.ndv831lc9uf0)
* [BlinkOn 6 presentation slides](https://docs.google.com/presentation/d/1gqK9F4lGAY3TZudAtdcxzMQNEE7PcuQrGu83No3l0lw/edit#slide=id.p)
* [Architecture design doc](https://docs.google.com/document/d/11zIkKkLBocofGgoTeeyibB2TZ_k7nR78v7kNelCatUE)
