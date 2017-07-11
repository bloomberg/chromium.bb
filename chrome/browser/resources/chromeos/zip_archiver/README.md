# ZIP Unpacker extension

This is the ZIP Unpacker extension used in Chrome OS to support reading and
unpacking of zip archives.

## Build steps

### NaCl SDK

Since the code is built with [NaCl](https://developer.chrome.com/native-client/),
you'll need its toolchain.

```
$ cd third-party
$ make nacl_sdk
```

### Webports (a.k.a. NaCl ports)

We'll use libraries from [webports](https://chromium.googlesource.com/webports/).

```
$ cd third-party
$ make depot_tools
$ make webports
```

### npm Setup

First install [npm](https://www.npmjs.com/) using your normal packaging system.
On Debian, you'll want something like:

```
$ sudo apt-get install npm
```

Then install the npm modules that we require.  Do this in the root of the
unpacker repo.

```
$ npm install bower 'vulcanize@<0.8'
```

### Unpacker Build

Once done, install the [libarchive-fork/] from [third-party/] of the unpacker
project. Note that you cannot use [libarchive] nor libarchive-dev packages from
webports at this moment, as not all patches in the fork are upstreamed.

```
$ cd third-party
$ make libarchive-fork
```

Polymer is used for UI. In order to fetch it, in the same directory type:

```
$ make polymer
```

Build the PNaCl module.

```
$ cd unpacker
$ make [debug]
```

## Use

The package can be found in the release or debug directory.  You can run it
directly from there using Chrome's "Load unpacked extension" feature, or you
can zip it up for posting to the Chrome Web Store.

```
$ zip -r release.zip release/
```

Once it's loaded, you should be able to open ZIP archives in the Files app.

## Source Layout

Paths that aren't linked below are dynamically created at build time.

* node_modules/: All the locally installed npm modules used for building.
* [third-party/]: The source for third-party NaCl & Polymer code.
  * [libarchive-fork/]: The [libarchive] NaCl module (w/custom patches).
  * [polymer/]: [Polymer](https://www.polymer-project.org/) code for UI objects.
* [unpacker/]: The extension CSS/HTML/JS/NaCl source code.
  * [cpp/]: The NaCl module source.
  * [css/]: Any CSS needed for styling UI.
  * debug/: A debug build of the Chrome extension.
  * [html/]: Any HTML needed for UI.
  * [icons/]: Various extension images.
  * [js/]: The JavaScript code.
  * [_locales/]: [Translations](https://developer.chrome.com/extensions/i18n) of strings shown to the user.
  * pnacl/: Compiled NaCl objects & module (debug & release).
  * release/: A release build of the Chrome extension.
* [unpacker-test/]: Code for running NaCl & JavaScript unittests.

### NaCl/JS Life Cycle

Some high level points to remember: the JS side reacts to user events and is
the only part that has access to actual data on disk.  It uses the NaCl module
to do all the data parsing (e.g. gzip & tar), but it has to both send a request
to the module ("parse this archive"), and respond to requests from the module
when the module needs to read actual bytes on disk.

When the extension loads, [background.js] registers everything and goes idle.

When the Files app wants to mount an archive, callbacks in [app.js]
`unpacker.app` are called to initialize the NaCl runtime.  Creates an
`unpacker.Volume` object for each mounted archive.

Requests on the archive (directory listing, metadata lookups, reading files)
are routed through [app.js] `unpacker.app` and to [volume.js] `unpacker.Volume`.
Then they are sent to the low level [decompressor.js] `unpacker.Decompressor`
which talks to the NaCl module using the [request.js] `unpacker.request`
protocol.  Responses are passed back up.

When the NaCl module is loaded, [module.cc] `NaclArchiveModule` is instantiated.
That instantiates `NaclArchiveInstance` for initial JS message entry points.
It instantiates `JavaScriptMessageSender` for sending requests back to JS.

When JS requests come in, [module.cc] `NaclArchiveInstance` will create
[volume.h] `Volume` objects on the fly, and pass requests down to them (using
the protocol defined in [request.h] `request::*`).

[volume.h] `Volume` objects in turn use the [volume_archive.h] `VolumeArchive`
abstract interface to handle requests from the JS side (using the protocol
defined in [request.h] `request:**`).  This way the lower levels don't have to
deal with JS directly.

[volume_archive_libarchive.cc] `VolumeArchiveLibarchive` implements the
`VolumeArchive` interface and uses [libarchive] as its backend to do all the
decompression & archive format processing.

But NaCl code doesn't have access to any files or data itself.  So the
[volume_reader.h] `VolumeReader` abstract interface is passed to it to provide
the low level data read functions.  The [volume_reader_javascript_stream.cc]
`VolumeReaderJavaScriptStream` implements that by passing requests back up
to the JS side via the [javascript_requestor_interface.h]
`JavaScriptRequestorInterface` interface (which was passed down to it).

So requests (mount an archive, read a file, etc...) generally follow the path:

* JavaScript -> `request::*`
  * [module.cc] `NaclArchiveModule` -> `NaclArchiveModule` -> `request::*`
    * [volume.cc] `Volume`
      * [volume_archive.h] `VolumeArchive`
        * [volume_archive_libarchive.cc] `VolumeArchiveLibarchive`
          * [volume_reader.h] `VolumeReader`
            * [volume_reader_javascript_stream.cc] `VolumeReaderJavaScriptStream`
              * [javascript_requestor_interface.h] `JavaScriptRequestorInterface`
                * [volume.cc] `JavaScriptRequestor`
                  * [javascript_message_sender_interface.h] `JavaScriptMessageSenderInterface`
                    * [module.cc] `JavaScriptMessageSender` -> `request::*`
                      * JavaScript (to read actual bytes of data)

Then once `VolumeArchive` has processed the raw data stream, it can return
results to the `Volume` object which takes care of posting JS status messages
back to the Chrome side.

#### Source Layout

Here's the JavaScript code that matters.  A few files have very specific
purposes and can be ignored at a high level, so they're in a separate section.

* [background.js]
  * Main entry point.
  * Initializes the module/runtime.
  * Registers the extension with Chrome filesystem/runtime.
* [app.js] `unpacker.app`
  * Main runtime for the extension.
  * Loads/unloads NaCl modules on demand (to save runtime memory).
  * Loads/unloads volumes as Chrome has requested.
  * Responds to Chrome filesystem callbacks.
  * Passes data back to Chrome from `unpacker.Volume` objects.
* [volume.js] `unpacker.Volume`
  * Every mounted archive has a `unpacker.Volume` instance.
  * Provides high level interface to requests (like reading files & metadata).
* [decompressor.js] `unpacker.Decompressor`
  * Provides low level interface for `unpacker.Volume` requests.
  * Talks to the NaCl module using the `unpacker.request` protocol.
* [request.js] `unpacker.request`
  * Handle the JS<->NaCl protocol communication.
* [passphrase-manager.js] `unpacker.PassphraseManager`
  * Interface for plumbing password requests between UI & JS & NaCl.

These are the boilerplate/simple JavaScript files you can generally ignore.

* [build-config.js]
  * Unused?
* [passphrase-dialog.js]
  * Polymer code for managing the password dialog.
* [types.js] `unpacker.types`
  * Basic file for setting up custom types/constants.
* [unpacker.js] `unpacker`
  * Basic file for setting up `unpacker.*` namespace.

Here's the NaCl layout.

* [module.cc]
  * Main entry point to the module.
  * Implements the ppapi interface.
  * Routes requests between the JS & NaCl layers.
  * Implements the `JavaScriptMessageSenderInterface` interface so the rest of
    NaCl code can easily send messages back up.
* [javascript_message_sender_interface.h]
  * API for the nacl code to easily send messages back up.
  * Implemented in [module.cc].
* [request.cc] [request.h]
  * Defines the protocol used to communicate between JS & NaCl.
* [javascript_requestor_interface.h]
  * Abstract `JavaScriptRequestorInterface` interface for talking to JS side.
  * Implemented in [volume.cc].
* [volume.cc] [volume.h]
  * Defines the `Volume` class that encompasses a high level volume.
  * Every mount request has a Volume.
  * Takes care of plumbing JS requests to lower `VolumeArchive`.
  * Implements `JavaScriptRequestorInterface` interface.
* [volume_archive.h]
  * Abstract `VolumeArchive` interface for handling specific archive formats.
* [volume_archive_libarchive.cc] [volume_archive_libarchive.h]
  * Implements `VolumeArchive` using the libarchive project.
* [volume_reader.h]
  * Abstract `VolumeReader` interface for low level reading of data.
  * Used to read raw streams of data from somewhere.
* [volume_reader_javascript_stream.cc] [volume_reader_javascript_stream.h]
  * Implements `VolumeReader`.
  * Uses `JavaScriptRequestorInterface` to get data from the JS side.

## Debugging

To see debug messages open chrome from a terminal and check the output.
For output redirection see
https://developer.chrome.com/native-client/devguide/devcycle/debugging.

## Testing

Install [Karma](http://karma-runner.github.io/0.12/index.html) for tests
runner, [Mocha](http://visionmedia.github.io/mocha/) for asynchronous testings,
[Chai](http://chaijs.com/) for assertions, and [Sinon](http://sinonjs.org/) for
spies and stubs.

```
$ npm install --save-dev \
  karma karma-chrome-launcher karma-cli \
  mocha karma-mocha karma-chai chai karma-sinon sinon

# Run tests:
$ cd unpacker-test
$ ./run_js_tests.sh  # JavaScript tests.
$ ./run_cpp_tests.sh  # C++ tests.

# Check JavaScript code using the Closure JS Compiler.
# See https://www.npmjs.com/package/closurecompiler
$ cd unpacker
$ npm install google-closure-compiler
$ bash check_js_for_errors.sh
```

[libarchive]: http://www.libarchive.org/

[third-party/]: ./third-party/
[libarchive-fork/]: ./third-party/libarchive-fork/
[polymer/]: ./third-party/polymer/
[unpacker/]: ./unpacker/
[cpp/]: ./unpacker/cpp/
[css/]: ./unpacker/css/
[html/]: ./unpacker/html/
[icons/]: ./unpacker/icons/
[js/]: ./unpacker/js/
[_locales/]: ./unpacker/_locales/
[unpacker-test/]: ./unpacker-test/

[javascript_message_sender_interface.h]: ./unpacker/cpp/javascript_message_sender_interface.h
[javascript_requestor_interface.h]: ./unpacker/cpp/javascript_requestor_interface.h
[module.cc]: ./unpacker/cpp/module.cc
[request.cc]: ./unpacker/cpp/request.cc
[request.h]: ./unpacker/cpp/request.h
[volume_archive.h]: ./unpacker/cpp/volume_archive.h
[volume_archive_libarchive.cc]: ./unpacker/cpp/volume_archive_libarchive.cc
[volume_archive_libarchive.h]: ./unpacker/cpp/volume_archive_libarchive.h
[volume.cc]: ./unpacker/cpp/volume.cc
[volume.h]: ./unpacker/cpp/volume.h
[volume_reader.h]: ./unpacker/cpp/volume_reader.h
[volume_reader_javascript_stream.cc]: ./unpacker/cpp/volume_reader_javascript_stream.cc
[volume_reader_javascript_stream.h]: ./unpacker/cpp/volume_reader_javascript_stream.h

[app.js]: ./unpacker/js/app.js
[background.js]: ./unpacker/js/background.js
[build-config.js]: ./unpacker/js/build-config.js
[decompressor.js]: ./unpacker/js/decompressor.js
[passphrase-dialog.js]: ./unpacker/js/passphrase-dialog.js
[passphrase-manager.js]: ./unpacker/js/passphrase-manager.js
[request.js]: ./unpacker/js/request.js
[types.js]: ./unpacker/js/types.js
[unpacker.js]: ./unpacker/js/unpacker.js
[volume.js]: ./unpacker/js/volume.js
