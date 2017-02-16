# ChromeVox (for developers)

ChromeVox is the built-in screen reader on Chrome OS. It was originally
developed as a separate extension but now the code lives inside of the Chromium
tree and it's built as part of Chrome OS.

To start or stop ChromeVox on Chrome OS, press Ctrl+Alt+Z at any time.

## Developer Info

Code location: ```chrome/browser/resources/chromeos/chromevox```

Ninja target: it's built as part of "chrome", but you can build and run
chromevox_tests to test it (Chrome OS target only - you must have target_os =
"chromeos" in your GN args first).

## Developing On Linux

ChromeVox for Chrome OS development is done on Linux.

See [ChromeVox on Desktop Linux](chromevox_on_desktop_linux.md)
for more information.

## ChromeVox Next

ChromeVox Next is the code name we use for a major new rewrite to ChromeVox that
uses the automation API instead of content scripts. The code is part of
ChromeVox (unique ChromeVox Next code is found in
chrome/browser/resources/chromeos/chromevox/cvox2).

ChromeVox contains all of the classic and next code in the same codebase, it
switches its behavior dynamically based on the mode:

* Next: as of version 56 of Chrome/Chrome OS, this is default. ChromeVox uses new key/braille bindings, earcons, speech/braille output style, the Next engine (Automation API), and other major/minor improvements
* Next Compat: in order to maintain compatibility with some clients of the ChromeVox Classic js APIs, some sites have been whitelisted for this mode. ChromeVox will inject classic content scripts, but expose a Next-like user experience (like above)
* Classic: as of version 56 of Chrome/Chrome OS, this mode gets enabled via a keyboard toggle Search+Q. Once enabled, ChromeVox will behave like it did in the past including keyboard bindings, earcons, speech/braille output style, and the underlying engine (content scripts).
* Classic compat for some sites that require Next, while running in Classic, ChromeVox will use the Next engine but expose a Classic user experience (like above)

Once it's ready, the plan is to retire everything other than Next mode.

## ChromeVox Next

To test ChromeVox Next, click on the Gear icon in the upper-right of the screen
to open the ChromeVox options (or press the keyboard shortcut Search+Shift+O, O)
and then click the box to opt into ChromeVox Next.

If you are running m56 or later, you already have ChromeVox Next on by
default. To switch back to Classic, press Search+Q.

## Debugging ChromeVox

There are options available that may assist in debugging ChromeVox. Here are a
few use cases.

### Feature development

When developing a new feature, it may be helpful to save time by not having to
go through a compile cycle. This can be achieved by setting
```chromevox_compress_js``` to 0 in
chrome/browser/resources/chromeos/chromevox/BUILD.gn, or by using a debug build.

In a debug build or with chromevox_compress_js off, the unflattened files in the
Chrome out directory (e.g. out/Release/resources/chromeos/chromevox/). Now you
can hack directly on the copy of ChromeVox in out/ and toggle ChromeVox to pick
up your changes (via Ctrl+Alt+Z).

### Fixing bugs

The easiest way to debug ChromeVox is from an external browser. Start Chrome
with this command-line flag:

```out/Release/chrome --remote-debugging-port=9222```

Now open http://localhost:9222 in a separate instance of the browser, and debug the ChromeVox extension background page from there.

Another option is to use emacs jade (available through -mx
package-list-packages).

It also talks to localhost:9222 but integrates more tightly into emacs instead.

Another option is to use the built-in developer console. Go to the
ChromeVox options page with Search+Shift+o, o; then, substitute the
“options.html” path with “background.html”, and then open up the
inspector.

### Running tests

Build the chromevox_tests target. To run
lots of tests in parallel, run it like this:

```out/Release/chromevox_tests --test-launcher-jobs=20```

Use a test filter if you only want to run some of the tests from a
particular test suite - for example, most of the ChromeVox Next tests
have "E2E" in them (for "end-to-end"), so to only run those:

```out/Release/chromevox_tests --test-launcher-jobs=20 --gtest_filter="*E2E*"```

## ChromeVox for other platforms

ChromeVox can be run as an installable extension, separate from a
linux Chrome OS build.

### From source

chrome/browser/resources/chromeos/chromevox/tools has the required scripts that pack ChromeVox as an extension and make any necessary manifest changes.

### From Webstore

Alternatively, the webstore has the stable version of ChromeVox.

To install without interacting with the webstore UI, place the
following json block in
/opt/google/chrome-unstable/extensions/kgejglhpjiefppelpmljglcjbhoiplfn.json

```
{
"external_update_url": "https://clients2.google.com/service/update2/crx"
}
```

If you're using the desktop Linux version of Chrome, we recommend you
use Voxin for speech. Run chrome with: “google-chrome
--enable-speech-dispatcher” and select a voice provided by the speechd
package from the ChromeVox options page (ChromeVox+o, o). As of the
latest revision of Chrome 44, speechd support has become stable enough
to use with ChromeVox, but still requires the flag.

In the ChromeVox options page, select the flat keymap and use sticky
mode (double press quickly of insert) to emulate a modal screen
reader.
