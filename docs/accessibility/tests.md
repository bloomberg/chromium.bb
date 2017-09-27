# Accessibility

Here's a quick overview of all of the locations in the codebase where
you'll find accessibility tests, and a brief overview of the purpose of
all of them.

## Layout Tests

This is the primary place where we test accessibility code in Blink. This
code should have no platform-specific code. Use this to test anything
where there's any interesting web platform logic, or where you need to be
able to query things synchronously from the renderer thread to test them.

Don't add tests for trivial features like ARIA attributes that we just
expose directly to the next layer up. In those cases the Blink tests are
trivial and it's more valuable to test these features at a higher level
where we can ensure they actually work.

Note that many of these tests are inherited from WebKit and the coding style
has evolved a lot since then. Look for more recent tests as a guide if writing
a new one.

Test files:
```
third_party/WebKit/LayoutTests/accessibility
```

Source code to AccessibilityController and WebAXObjectProxy:
```
content/shell/test_runner
```

To run all accessibility LayoutTests:
```
ninja -C out/release blink_tests
third_party/WebKit/Tools/Scripts/run-webkit-tests --build-directory=out --target=release accessibility/
```

To run just one test by itself without the script:
```
ninja -C out/release blink_tests
out/release/content_shell --run-layout-test third_party/WebKit/LayoutTests/accessibility/name-calc-inputs.html
```

## DumpAccessibilityTree tests

This is the best way to write both cross-platform and platform-specific tests
using only an input HTML file, some magic strings to describe what attributes
you're interested in, and one or more expectation files to enable checking
whether the resulting accessibility tree is correct or not. In particular,
almost no test code is required.

[More documentation on DumpAccessibilityTree](../../content/test/data/accessibility/readme.md)

Test files:
```
content/test/data/accessibility
```

Test runner:
```
content/browser/accessibility/dump_accessibility_tree_browsertest.cc
```

To run all tests:
```
ninja -C out/release content_browsertests
out/release/content_browsertests --gtest_filter="DumpAccessibilityTree*"
```

## Other content_browsertests

There are many other tests in content/ that relate to accessibility.
All of these tests work by launching a full multi-process browser shell,
loading a web page in a renderer, then accessing the resulting accessibility
tree from the browser process, and running some test from there.

To run all tests:
```
ninja -C out/release content_browsertests
out/release/content_browsertests --gtest_filter="*ccessib*"
```

## Accessibility unittests

This tests the core accessibility code that's shared by both web and non-web
accessibility infrastructure.

Code location:
```
ui/accessibility
```

To run all tests:
```
ninja -C out/release accessibility_unittests
out/release/accessibility_unittests
```

## ChromeVox tests

You must build with ```target_os = "chromeos"``` in your GN args.

To run all tests:
```
ninja -C out/release chromevox_tests
out/release/chromevox_tests --test-launcher-jobs=10
```

### Select-To-Speak tests

```
ninja -C out/release unit_tests
out/release/unit_tests --gtest_filter="*SelectToSpeak*"
```

## Other locations of accessibility tests:

Even this isn't a complete list. The tests described above cover more
than 90% of the accessibility tests, and the remainder are scattered
throughout the codebase. Here are a few other locations to check:

```
chrome/android/javatests/src/org/chromium/chrome/browser/accessibility
chrome/browser/accessibility
chrome/browser/chromeos/accessibility/
ui/chromeos
ui/views/accessibility
```

## Helpful flags:

Across all tests there are some helpful flags that will make testing easier.

```
--test-launcher-jobs=10  # This will run stuff in parallel and make flakes more obvious
```

```
--gtest_filter="*Cats*"  # Filter which tests run. Takes a wildcard (*) optionally.
```
