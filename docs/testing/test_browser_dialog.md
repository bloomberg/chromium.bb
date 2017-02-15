# Testing Chrome browser dialogs with TestBrowserDialog

\#include "[chrome/browser/ui/test/test_browser_dialog.h]"

`TestBrowserDialog` provides a way to register an `InProcessBrowserTest` testing
harness with a framework that invokes Chrome browser dialogs in a consistent
way. It optionally provides a way to invoke dialogs "interactively". This allows
screenshots to be generated easily, with the same test data, to assist with UI
review. It also provides a registry of dialogs so they can be systematically
checked for subtle changes and regressions.

[TOC]

## How to register a dialog

If registering an existing dialog, there's a chance it already has a test
harness inheriting, using, or with `typedef InProcessBrowserTest` (or a
descendant of it). If so, using `TestBrowserDialog` is straightforward. Assume
the existing InProcessBrowserTest is in `foo_dialog_browsertest.cc`:

    class FooDialogTest : public InProcessBrowserTest { ...

Change this to inherit from DialogBrowserTest, and override
`ShowDialog(std::string)`. See [Advanced Usage](#Advanced-Usage) for details.

```cpp
class FooDialogTest : public DialogBrowserTest {
 public:
  ..
  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    /* Show dialog attached to browser() and leave it open. */
  }
  ..
};
```

Then add test invocations using the usual GTest macros, in
`foo_dialog_browsertest.cc`:

```cpp
IN_PROC_BROWSER_TEST_F(FooDialogTest, InvokeDialog_default) {
  RunDialog();
}
```

Notes:

*   The body of the test is always just "`RunDialog();`".
*   "`default`" is the `std::string` passed to `ShowDialog()` and can be
    customized. See
    [Testing additional dialog "styles"](#Testing-additional-dialog-styles).
*   The text before `default` (in this case) must always be "`InvokeDialog_`".

### Concrete examples

*   [chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc]
*   [chrome/browser/ui/autofill/card_unmask_prompt_view_browsertest.cc]
*   [chrome/browser/ui/collected_cookies_browsertest.cc]
*   [chrome/browser/ui/update_chrome_dialog_browsertest.cc]

##  Running the tests

List the available dialogs with

    $ ./browser_tests --gtest_filter=BrowserDialogTest.Invoke

E.g. `FooDialogTest.InvokeDialog_default` should be listed. To show the dialog
interactively, run

    $ ./browser_tests --gtest_filter=BrowserDialogTest.Invoke --interactive \
      --dialog=FooDialogTest.InvokeDialog_default

### Implementation

`BrowserDialogTest.Invoke` searches for gtests that have "`InvokeDialog_`"  in
their name, so they can be collected in a list. Providing a `--dialog` argument
will invoke that test case in a subprocess. Including `--interactive` will set
up an environment for that subprocess that allows interactivity, e.g., to take
screenshots. The test ends once the dialog is dismissed.

The `FooDialogTest.InvokeDialog_default` test case **will still be run in the
usual browser_tests test suite**. Ensure it passes, and isn’t flaky. This will
give your dialog some regression test coverage. `RunDialog()` checks to ensure a
dialog is actually created when it invokes `ShowDialog("default")`.

### BrowserDialogTest.Invoke

This is also run in browser_tests but, when run that way, the test case just
lists the registered test harnesses (it does *not* iterate over them). A
subprocess is never created unless --dialog is passed on the command line.

## Advanced Usage

If your test harness inherits from a descendant of `InProcessBrowserTest` (one
example: [ExtensionBrowserTest]) then the `SupportsTestDialog<>` template is
provided. E.g.

```cpp
class ExtensionInstallDialogViewTestBase : public ExtensionBrowserTest { ...
```

becomes

```cpp
class ExtensionInstallDialogViewTestBase :
    public SupportsTestDialog<ExtensionBrowserTest> { ...
```

### Testing additional dialog "styles"

Add additional test cases, with a different string after "`InvokeDialog_`".
Example:

```cpp
IN_PROC_BROWSER_TEST_F(CardUnmaskViewBrowserTest, InvokeDialog_expired) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskViewBrowserTest, InvokeDialog_valid) {
  RunDialog();
}
```

The strings "`expired`" or “`valid`” will be given as arguments to
`ShowDialog(std::string)`.

## Rationale

Bug reference: [Issue 654151](http://crbug.com/654151).

Chrome has a lot of browser dialogs; often for obscure use-cases and often hard
to invoke. It has traditionally been difficult to be systematic while checking
dialogs for possible regressions. For example, to investigate changes to shared
layout parameters which are testable only with visual inspection.

For Chrome UI review, screenshots need to be taken. Iterating over all the
"styles" that a dialog may appear with is fiddly. E.g. a login or particular web
server setup may be required. It’s important to provide a consistent “look” for
UI review (e.g. same test data, same browser size, anchoring position, etc.).

Some dialogs lack tests. Some dialogs have zero coverage on the bots. Dialogs
can have tricky lifetimes and common mistakes are repeated. TestBrowserDialog
runs simple "Show dialog" regression tests and can be extended to do more.

Even discovering the full set of dialogs present for each platform in Chrome is
[difficult](http://crbug.com/686239).

### Why browser_tests?

*   `browser_tests` already provides a `browser()->window()` of a consistent
    size that can be used as a dialog anchor and to take screenshots for UI
    review.
    *   UI review have requested that screenshots be provided with the entire
        browser window so that the relative size of the dialog/change under
        review can be assessed.

*   Some dialogs already have a test harness with appropriate setup (e.g. test
    data) running in browser_tests.
    *   Supporting `BrowserDialogTest` should require minimal setup and minimal
        ongoing maintenance.

*   An alternative is to maintain a working end-to-end build target executable
    to do this, but this has additional costs (and is hard).
    *    E.g. setup/teardown of low-level functions (
         `InitializeGLOneOffPlatform()`,
         `MaterialDesignController::Initialize()`, etc.).

*   Why not chrome.exe?
    *   E.g. a scrappy chrome:// page with links to invoke dialogs would be
        great!
    *   But...
        *   A dialog may have test data (e.g. credit card info) which shouldn’t
        be in the release build.
        *   A dialog may use EmbeddedTestServer.
        *   Higher maintenance cost - can’t leverage existing test harnesses.

## Future Work

*   Opt in more dialogs!
    *    Eventually, all of them.
    *    A `BrowserDialogTest` for every descendant of `views::DialogDelegate`.

*   Automatically generate screenshots (for each platform, in various languages)
    *    Build upon [CL 2008283002](https://codereview.chromium.org/2008283002/)

*   (maybe) Try removing the subprocess
    *    Probably requires altering the browser_test suite code directly rather
         than just adding a test case as in the current approach

*   An automated test suite for dialogs
    *    Test various ways to dismiss or hide a dialog
         *    e.g. native close (via taskbar?)
         *    close parent window (possibly via task bar)
         *    close parent tab
         *    switch tabs
         *    close via `DialogClientView::AcceptWindow` (and `CancelWindow`)
         *    close via `Widget::Close`
         *    close via `Widget::CloseNow`
    *    Drag tab off browser into a new window
    *    Fullscreen that may create a new window/parent

*   Find obscure workflows for invoking dialogs that have no test coverage and
    cause crashes (e.g. [http://crrev.com/426302](http://crrev.com/426302))
    *   Supporting window-modal dialogs with a null parent window.

*   Find memory leaks, e.g. [http://crrev.com/432320](http://crrev.com/432320)
    *   "Fix memory leak for extension uninstall dialog".

## Appendix: Sample output

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserDialogTest.Invoke**
```
Note: Google Test filter = BrowserDialogTest.Invoke
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from BrowserDialogTest
[ RUN      ] BrowserDialogTest.Invoke
[26879:775:0207/134949.118352:30434675...:INFO:browser_dialog_browsertest.cc(46)
Pass one of the following after --dialog=
  AskGoogleForSuggestionsDialogTest.InvokeDialog_default
  CardUnmaskPromptViewBrowserTest.InvokeDialog_expired
  CardUnmaskPromptViewBrowserTest.InvokeDialog_valid
  CollectedCookiesTestMd.InvokeDialog_default
  UpdateRecommendedDialogTest.InvokeDialog_default
/* lots more will eventually be listed here */
[       OK ] BrowserDialogTest.Invoke (0 ms)
[----------] 1 test from BrowserDialogTest (0 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1 ms total)
[  PASSED  ] 1 test.
[1/1] BrowserDialogTest.Invoke (334 ms)
SUCCESS: all tests passed.
```

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserDialogTest.Invoke
--dialog=CardUnmaskPromptViewBrowserTest.InvokeDialog_expired**

```
Note: Google Test filter = BrowserDialogTest.Invoke
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from BrowserDialogTest
[ RUN      ] BrowserDialogTest.Invoke
Note: Google Test filter = CardUnmaskPromptViewBrowserTest.InvokeDefault
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from CardUnmaskPromptViewBrowserTest, where TypeParam =
[ RUN      ] CardUnmaskPromptViewBrowserTest.InvokeDialog_expired
/* 7 lines of uninteresting log spam */
[       OK ] CardUnmaskPromptViewBrowserTest.InvokeDialog_expired (1324 ms)
[----------] 1 test from CardUnmaskPromptViewBrowserTest (1324 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1325 ms total)
[  PASSED  ] 1 test.
[       OK ] BrowserDialogTest.Invoke (1642 ms)
[----------] 1 test from BrowserDialogTest (1642 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1642 ms total)
[  PASSED  ] 1 test.
[1/1] BrowserDialogTest.Invoke (2111 ms)
SUCCESS: all tests passed.
```

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserDialogTest.Invoke
--dialog=CardUnmaskPromptViewBrowserTest.InvokeDialog_expired --interactive**
```
/*
 * Output as above, except the test are not interleaved, and the browser window
 * should remain open until the dialog is dismissed
 */
```

[chrome/browser/ui/test/test_browser_dialog.h]: https://cs.chromium.org/chromium/src/chrome/browser/ui/test/test_browser_dialog.h
[chrome/browser/ui/autofill/card_unmask_prompt_view_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/autofill/card_unmask_prompt_view_browsertest.cc?l=104&q=ShowDialog
[chrome/browser/ui/collected_cookies_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/collected_cookies_browsertest.cc?l=26&q=ShowDialog
[chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc?l=18&q=ShowDialog
[chrome/browser/ui/update_chrome_dialog_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/update_chrome_dialog_browsertest.cc?l=15&q=ShowDialog
[ExtensionBrowserTest]: https://cs.chromium.org/chromium/src/chrome/browser/extensions/extension_browsertest.h?q=extensionbrowsertest&l=40
