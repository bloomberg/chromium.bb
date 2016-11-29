# Writing Layout Tests

_Layout tests_ is a bit of a misnomer. This term is
[a part of our WebKit heritage](https://webkit.org/blog/1452/layout-tests-theory/),
and we use it to refer to every test that is written as a Web page (HTML, SVG,
or XHTML) and lives in
[third_party/WebKit/LayoutTests/](../../third_party/WebKit/LayoutTests).

[TOC]

## Overview

Layout tests should be used to accomplish one of the following goals:

1. The entire surface of Blink that is exposed to the Web should be covered by
   tests that we contribute to the
   [Web Platform Tests Project](https://github.com/w3c/web-platform-tests)
   (WPT). This helps us avoid regressions, and helps us identify Web Platform
   areas where the major browsers don't have interoperable implementations.
   Furthermore, by contributing to projects such as WPT, we share the burden of
   writing tests with the other browser vendors, and we help all the browsers
   get better. This is very much in line with our goal to move the Web forward.
2. When a Blink feature cannot be tested using the tools provided by WPT, and
   cannot be easily covered by
   [C++ unit tests](https://cs.chromium.org/chromium/src/third_party/WebKit/Source/web/tests/?q=webframetest&sq=package:chromium&type=cs),
   the feature must be covered by layout tests, to avoid unexpected regressions.
   These tests will use Blink-specific testing APIs that are only available in
   [content_shell](./layout_tests_in_content_shell.md).

*** promo
If you know that Blink layout tests are upstreamed to other projects, such as
[test262](https://github.com/tc39/test262), please update this document. Most
importantly, our guidelines should to make it easy for our tests to be
upstreamed. The `blink-dev` mailing list will be happy to help you harmonize our
current guidelines with communal test repositories.
***

### Test Types

There are four broad types of layout tests, listed in the order of preference.

* *JavaScript Tests* are the layout test implementation of
  [xUnit tests](https://en.wikipedia.org/wiki/XUnit). These tests contain
  assertions written in JavaScript, and pass if the assertions evaluate to
  true.
* *Reference Tests* render a test page and a reference page, and pass if the two
  renderings are identical, according to a pixel-by-pixel comparison. These
  tests are less robust, harder to debug, and significantly slower than
  JavaScript tests, and are only used when JavaScript tests are insufficient,
  such as when testing paint code.
* *Pixel Tests* render a test page and compare the result against a pre-rendered
  baseline image in the repository. Pixel tests are less robust than all
  alternatives listed above, because the rendering of a page is influenced by
  many factors such as the host computer's graphics card and driver, the
  platform's text rendering system, and various user-configurable operating
  system settings. For this reason, it is common for a pixel test to have a
  different reference image for each platform that Blink is tested on. Pixel
  tests are least preferred, because the reference images are
  [quite cumbersome to manage](./layout_test_expectations.md).
* *Dump Render Tree (DRT) Tests* output a textual representation of the render
  tree, which is the key data structure in Blink's page rendering system. The
  test passes if the output matches a baseline text file in the repository. In
  addition to their text result, DRT tests can also produce an image result
  which is compared to an image baseline, similarly to pixel tests (described
  above). A DRT test with two results (text and image) passes if _both_ results
  match the baselines in the repository. DRT tests are less desirable than all
  the alternatives, because they depend on a browser implementation detail.

## General Principles

The principles below are adapted from
[Test the Web Forward's Test Format Guidelines](http://testthewebforward.org/docs/test-format-guidelines.html)
and
[WebKit's Wiki page on Writing good test cases](https://trac.webkit.org/wiki/Writing%20Layout%20Tests%20for%20DumpRenderTree).

* Tests should be **concise**, without compromising on the principles below.
  Every element and piece of code on the page should be necessary and relevant
  to what is being tested. For example, don't build a fully functional signup
  form if you only need a text field or a button.
      * Content needed to satisfy the principles below is considered necessary.
        For example, it is acceptable and desirable to add elements that make
        the test self-describing (see below), and to add code that makes the
        test more reliable (see below).
      * Content that makes test failures easier to debug is considered necessary
        (to maintaining a good development speed), and is both acceptable and
        desirable.
      * Conciseness is particularly important for reference tests and pixel
        tests, as the test pages are rendered in an 800x600px viewport. Having
        content outside the viewport is undesirable because the outside content
        does not get compared, and because the resulting scrollbars are
        platform-specific UI widgets, making the test results less reliable.

* Tests should be as **fast** as possible, without compromising on the
  principles below. Blink has several thousand layout tests that are run in
  parallel, and avoiding unnecessary delays is crucial to keeping our Commit
  Queue in good shape.
    * Avoid [window.setTimeout](https://developer.mozilla.org/en-US/docs/Web/API/WindowTimers/setTimeout),
      as it wastes time on the testing infrastructure. Instead, use specific
      event handlers, such as
      [window.onload](https://developer.mozilla.org/en-US/docs/Web/API/GlobalEventHandlers/onload),
      to decide when to advance to the next step in a test.

* Tests should be **reliable** and yield consistent results for a given
  implementation. Flaky tests slow down your fellow developers' debugging
  efforts and the Commit Queue.
    * `window.setTimeout` is again a primary offender here. Asides from wasting
      time on a fast system, tests that rely on fixed timeouts can fail when run
      on systems that are slower than expected.
    * Follow the guidelines in this
      [PSA on writing reliable layout tests](https://docs.google.com/document/d/1Yl4SnTLBWmY1O99_BTtQvuoffP8YM9HZx2YPkEsaduQ/edit).

* Tests should be **self-describing**, so that a project member can recognize
  whether a test passes or fails without having to read the specification of the
  feature being tested. `testharness.js` makes a test self-describing when used
  correctly, but tests that degrade to manual tests
  [must be carefully designed](http://testthewebforward.org/docs/test-style-guidelines.html)
  to be self-describing.

* Tests should require a **minimal** amount of cognitive effort to read and
  maintain.
    * Avoid depending on edge case behavior of features that aren't explicitly
      covered by the test. For example, except where testing parsing, tests
      should contain valid markup (no parsing errors).
    * Tests should provide as much relevant information as possible when
      failing. `testharness.js` tests should prefer
      [rich assert_ functions](https://github.com/w3c/testharness.js/blob/master/docs/api.md#list-of-assertions)
      to combining `assert_true()` with a boolean operator. Using appropriate
      `assert_` functions results in better diagnostic output when the assertion
      fails.
    * Prefer JavaScript's
      [===](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Operators/Comparison_Operators#Identity_strict_equality_())
      operator to
      [==](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Operators/Comparison_Operators#Equality_())
      so that readers don't have to reason about
      [type conversion](http://www.ecma-international.org/ecma-262/6.0/#sec-abstract-equality-comparison).

* Tests should be as **cross-platform** as reasonably possible. Avoid
  assumptions about device type, screen resolution, etc. Unavoidable assumptions
  should be documented.
    * When possible, tests should only use Web platform features, as specified
      in the relevant standards. When the Web platform's APIs are insufficient,
      tests should prefer to use WPT extended testing APIs, such as
      `wpt_automation`.
    * Test pages should use the HTML5 doctype (`<!doctype html>`) unless they
      specifically cover
      [quirks mode](https://developer.mozilla.org/docs/Quirks_Mode_and_Standards_Mode)
      behavior.
    * Tests should be written under the assumption that they will be upstreamed
      to the WPT project. For example, tests should follow the
      [WPT guidelines](http://testthewebforward.org/docs/writing-tests.html).
    * Tests that use Blink-specific testing APIs should feature-test for the
      presence of the testing APIs and degrade to
      [manual tests](http://testthewebforward.org/docs/manual-test.html) when
      the testing APIs are not present. _This is not currently enforced in code
      review. However, please keep in mind that a manual test can be debugged in
      the browser, whereas a test that does not degrade gracefully can only be
      debugged in the test runner._

* Tests must be **self-contained** and not depend on external network resources.
  Unless used by multiple test files, CSS and JavaScript should be inlined using
  `<style>` and `<script>` tags. Content shared by multiple tests should be
  placed in a `resources/` directory near the tests that share it. See below for
  using multiple origins in a test.

* Test **file names** should describe what is being tested. File names should
  use `snake-case`, but preserve the case of any embedded API names. For
  example, prefer `document-createElement.html` to
  `document-create-element.html`.

* Tests should prefer **modern features** in JavaScript and in the Web Platform.
    * Tests should use
      [strict mode](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Strict_mode)
      for all JavaScript, except when specifically testing sloppy mode behavior.
      Strict mode flags deprecated features and helps catch some errors, such as
      forgetting to declare variables.
    * JavaScript code should prefer
      [const](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Statements/const)
      and
      [let](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Statements/let)
      over `var`,
      [classes](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Classes)
      over other OOP constructs, and
      [Promises](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Promise)
      over other mechanisms for structuring asynchronous code.
    * The desire to use modern features must be balanced with the desire for
      cross-platform tests. Avoid using features that haven't been shipped by
      other developed major rendering engines (WebKit, Gecko, Edge). When
      unsure, check [caniuse.com](http://caniuse.com/).

* Tests should use the UTF-8 **character encoding**, which should be declared by
  `<meta charset=utf-8>`. This does not apply when specifically testing
  encodings.
      * At this time, code reviewers may choose to accept layout tests that do
        not have a `<meta charset>`, as long as the file content is pure ASCII.
        If going that route, please keep in mind that Firefox currently issues a
        dev tools warning for pages without a declared charset.

* Tests should aim to have a **coding style** that is consistent with
  [Google's JavaScript Style Guide](https://google.github.io/styleguide/jsguide.html),
  and
  [Google's HTML/CSS Style Guide](https://google.github.io/styleguide/htmlcssguide.xml),
  with the following exceptions.
    * Rules related to Google Closure and JSDoc do not apply.
    * Modern Web Platform and JavaScript features should be preferred to legacy
      constructs that target old browsers. For example, prefer `const` and `let`
      to `var`, and prefer `class` over other OOP constructs. This should be
      balanced with the desire to have cross-platform tests.
    * Concerns regarding buggy behavior in legacy browsers do not apply. For
      example, the garbage collection cycle note in the _Closures_ section does
      not apply.
    * Per the JavaScript guide, new tests should also follow any per-project
      style guide, such as the
      [ServiceWorker Tests Style guide](http://www.chromium.org/blink/serviceworker/testing).

*** note
This document intentionally uses _should_ a lot more than _must_, as defined in
[RFC 2119](https://www.ietf.org/rfc/rfc2119.txt). Writing layout tests is a
careful act of balancing many concerns, and this humble document cannot possibly
capture the context that rests in the head of an experienced Blink engineer.
***

## JavaScript Tests

Whenever possible, the testing criteria should be expressed in JavaScript. The
alternatives, which will be described in future sections, result in slower and
less reliable tests.

All new JavaScript tests should be written using the
[testharness.js](https://github.com/w3c/testharness.js/) testing framework. This
framework is used by the tests in the
[web-platform-tests](https://github.com/w3c/web-platform-tests) repository,
which is shared with all the other browser vendors, so `testharness.js` tests
are more accessible to browser developers.

As a shared framework, `testharness.js` enjoys high-quality documentation, such
as [a tutorial](http://testthewebforward.org/docs/testharness-tutorial.html) and
[API documentation](https://github.com/w3c/testharness.js/blob/master/docs/api.md).
Layout tests should follow the recommendations of the above documents.
Furthermore, layout tests should include relevant
[metadata](http://testthewebforward.org/docs/css-metadata.html). The
specification URL (in `<link rel="help">`) is almost always relevant, and is
incredibly helpful to a developer who needs to understand the test quickly.

Below is a skeleton for a JavaScript test embedded in an HTML page. Note that,
in order to follow the minimality guideline, the test omits the tags `<html>`,
`<head>`, and `<body>`, as they can be inferred by the HTML parser.

```html
<!doctype html>
<meta charset="utf-8">
<title>JavaScript: the true literal</title>
<link rel="help" href="https://tc39.github.io/ecma262/#sec-boolean-literals">
<meta name="assert" value="The true literal is equal to itself and immutable">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
'use strict';

// Synchronous test example.
test(() => {
  const value = true;
  assert_true(value, 'true literal');
  assert_equals(value.toString(), 'true', 'the string representation of true');
}, 'The literal true in a synchronous test case');

// Asynchronous test example.
async_test(t => {
  const originallyTrue = true;
  setTimeout(t.step_func_done(() => {
    assert_equals(originallyTrue, true);
  }), 0);
}, 'The literal true in a setTimeout callback');

// Promise test example.
promise_test(() => {
  return new Promise((resolve, reject) => {
    resolve(true);
  }).then(value => {
    assert_true(value);
  });
}, 'The literal true used to resolve a Promise');

</script>
```

Some points that are not immediately obvious from the example:

* The `<meta name="assert">` describes the purpose of the entire file, and
  is not redundant to `<title>`. Don't add a `<meta name="assert">` when the
  information in the `<title>` is sufficient.
* When calling an `assert_` function that compares two values, the first
  argument is the actual value (produced by the functionality being tested), and
  the second argument is the expected value (known good, golden). The order
  is important, because the testing harness relies on it to generate expressive
  error messages that are relied upon when debugging test failures.
* The assertion description (the string argument to `assert_` methods) conveys
  the way the actual value was obtained.
    * If the expected value doesn't make it clear, the assertion description
      should explain the desired behavior.
    * Test cases with a single assertion should omit the assertion's description
      when it is sufficiently clear.
* Each test case describes the circumstance that it tests, without being
  redundant.
    * Do not start test case descriptions with redundant terms like "Testing"
      or "Test for".
    * Test files with a single test case should omit the test case description.
      The file's `<title>` should be sufficient to describe the scenario being
      tested.
* Asynchronous tests have a few subtleties.
    * The `async_test` wrapper calls its function with a test case argument that
      is used to signal when the test case is done, and to connect assertion
      failures to the correct test.
    * `t.done()` must be called after all the test case's assertions have
      executed.
    * Test case assertions (actually, any callback code that can throw
      exceptions) must be wrapped in `t.step_func()` calls, so that
      assertion failures and exceptions can be traced back to the correct test
      case.
    * `t.step_func_done()` is a shortcut that combines `t.step_func()` with a
      `t.done()` call.

*** promo
Layout tests that load from `file://` origins must currently use relative paths
to point to
[/resources/testharness.js](../../third_party/WebKit/LayoutTests/resources/testharness.js)
and
[/resources/testharnessreport.js](../../third_party/WebKit/LayoutTests/resources/testharnessreport.js).
This is contrary to the WPT guidelines, which call for absolute paths.
This limitation does not apply to the tests in `LayoutTests/http`, which rely on
an HTTP server, or to the tests in `LayoutTests/imported/wpt`, which are
imported from the [WPT repository](https://github.com/w3c/web-platform-tests).
***

### WPT Supplemental Testing APIs

Some tests simply cannot be expressed using the Web Platform APIs. For example,
some tests that require a user to perform a gesture, such as a mouse click,
cannot be implemented using Web APIs. The WPT project covers some of these cases
via supplemental testing APIs.

*** promo
In many cases, the user gesture is not actually necessary. For example, many
event handling tests can use
[synthetic events](https://developer.mozilla.org/docs/Web/Guide/Events/Creating_and_triggering_events).
***

*** note
TODO: document wpt_automation. Manual tests might end up moving here.
***

### Relying on Blink-Specific Testing APIs

Tests that cannot be expressed using the Web Platform APIs or WPT's testing APIs
use Blink-specific testing APIs. These APIs are only available in
[content_shell](./layout_tests_in_content_shell.md), and should only be used as
a last resort.

A downside of Blink-specific APIs is that they are not as well documented as the
Web Platform features. Learning to use a Blink-specific feature requires finding
other tests that use it, or reading its source code.

For example, the most popular Blink-specific API is `testRunner`, which is
implemented in
[components/test_runner/test_runner.h](../../components/test_runner/test_runner.h)
and
[components/test_runner/test_runner.cpp](../../components/test_runner/test_runner.cpp).
By skimming the `TestRunnerBindings::Install` method, we learn that the
testRunner API is presented by the `window.testRunner` and
`window.layoutTestsController` objects, which are synonyms. Reading the
`TestRunnerBindings::GetObjectTemplateBuilder` method tells us what properties
are available on the `window.testRunner` object.

*** aside
`window.testRunner` is the preferred way to access the `testRunner` APIs.
`window.layoutTestsController` is still supported because it is used by
3rd-party tests.
***

*** note
`testRunner` is the most popular testing API because it is also used indirectly
by tests that stick to Web Platform APIs. The `testharnessreport.js` file in
`testharness.js` is specifically designated to hold glue code that connects
`testharness.js` to the testing environment. Our implementation is in
[third_party/WebKit/LayoutTests/resources/testharnessreport.js](../../third_party/WebKit/LayoutTests/resources/testharnessreport.js),
and uses the `testRunner` API.
***

See the [components/test_runner/](../../components/test_runner/) directory and
[WebKit's LayoutTests guide](https://trac.webkit.org/wiki/Writing%20Layout%20Tests%20for%20DumpRenderTree)
for other useful APIs. For example, `window.eventSender`
([components/test_runner/event_sender.h](../../components/test_runner/event_sender.h)
and
[components/test_runner/event_sender.cpp](../../components/test_runner/event_sender.cpp))
has methods that simulate events input such as keyboard / mouse input and
drag-and-drop.

Here is a UML diagram of how the `testRunner` bindings fit into Chromium.

[![UML of testRunner bindings configuring platform implementation](https://docs.google.com/drawings/u/1/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/export/svg?id=1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg&pageid=p)](https://docs.google.com/drawings/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/edit)
### Manual Tests

Whenever possible, tests that rely on (WPT's or Blink's) testing APIs should
also be usable as
[manual tests](http://testthewebforward.org/docs/manual-test.html). This makes
it easy to debug the test, and to check whether our behavior matches other
browsers.

*** note
The recommendation to have tests that depend on Blink-only testing APIs
gracefully degrade to manual tests is not currently enforced in code review.
When considering skipping this recommendation, please keep in mind that a manual
test can be debugged in the browser, whereas a test that does not degrade
gracefully can only be debugged in the test runner. Fellow project members and
future you will thank you for having your test work as a manual test.
***

Manual tests should minimize the chance of user error. This implies keeping the
manual steps to a minimum, and having simple and clear instructions that
describe all the configuration changes and user gestures that match the effect
of the Blink-specific APIs used by the test.

Below is an example of a fairly minimal test that uses a Blink-Specific API
(`window.eventSender`), and gracefully degrades to a manual test.

```html
<!doctype html>
<meta charset="utf-8">
<title>DOM: Event.isTrusted for UI events</title>
<link rel="help" href="https://dom.spec.whatwg.org/#dom-event-istrusted">
<link rel="help" href="https://dom.spec.whatwg.org/#constructing-events">
<meta name="assert"
    content="Event.isTrusted is true for events generated by user interaction">
<script src="../../resources/testharness.js"></script>
<script src="../../resources/testharnessreport.js"></script>

<p>Please click on the button below.</p>
<button>Click Me!</button>

<script>
'use strict';

setup({ explicit_timeout: true });

promise_test(() => {
  const button = document.querySelector('button');
  return new Promise((resolve, reject) => {
    const button = document.querySelector('button');
    button.addEventListener('click', (event) => {
      resolve(event);
    });

    if (window.eventSender) {
      eventSender.mouseMoveTo(button.offsetLeft, button.offsetTop);
      eventSender.mouseDown();
      eventSender.mouseUp();
    }
  }).then((clickEvent) => {
    assert_true(clickEvent.isTrusted);
  });

}, 'Click generated by user interaction');

</script>
```

The test exhibits the following desirable features:

* It has a second specification URL (`<link rel="help">`), because the paragraph
  that documents the tested feature (referenced by the primary URL) is not very
  informative on its own.
* It links to the
  [WHATWG Living Standard](https://wiki.whatwg.org/wiki/FAQ#What_does_.22Living_Standard.22_mean.3F),
  rather than to a frozen version of the specification.
* It contains clear instructions for manually triggering the test conditions.
  The test starts with a paragraph (`<p>`) that tells the tester exactly what to
  do, and the `<button>` that needs to be clicked is clearly labeled.
* It disables the timeout mechanism built into `testharness.js` by calling
  `setup({ explicit_timeout: true });`
* It checks for the presence of the Blink-specific testing APIs
  (`window.eventSender`) before invoking them. The test does not automatically
  fail when the APIs are not present.
* It uses [Promises](https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Promise)
  to separate the test setup from the assertions. This is particularly helpful
  for manual tests that depend on a sequence of events to occur, as Promises
  offer a composable way to express waiting for asynchronous events that avoids
  [callback hell](http://stackabuse.com/avoiding-callback-hell-in-node-js/).

Notice that the test is pretty heavy compared to a minimal JavaScript test that
does not rely on testing APIs. Only use testing APIs when the desired testing
conditions cannot be set up using Web Platform APIs.

### Text Test Baselines

By default, all the test cases in a file that uses `testharness.js` are expected
to pass. However, in some cases, we prefer to add failing test cases to the
repository, so that we can be notified when the failure modes change (e.g., we
want to know if a test starts crashing rather than returning incorrect output).
In these situations, a test file will be accompanied by a baseline, which is an
`-expected.txt` file that contains the test's expected output.

The baselines are generated automatically when appropriate by
`run-webkit-tests`, which is described [here](./layout_tests.md), and by the
[rebaselining tools](./layout_test_expectations.md).

Text baselines for `testharness.js` should be avoided, as having a text baseline
associated with a `testharness.js` indicates the presence of a bug. For this
reason, CLs that add text baselines must include a
[crbug.com](https://crbug.com) link for an issue tracking the removal of the
text expectations.

* When creating tests that will be upstreamed to WPT, and Blink's current
  behavior does not match the specification that is being tested, a text
  baseline is necessary. Remember to create an issue tracking the expectation's
  removal, and to link the issue in the CL description.
* Layout tests that cannot be upstreamed to WPT should use JavaScript to
  document Blink's current behavior, rather than using JavaScript to document
  desired behavior and a text file to document current behavior.

### The js-test.js Legacy Harness

*** promo
For historical reasons, older tests are written using the `js-test` harness.
This harness is **deprecated**, and should not be used for new tests.
***

If you need to understand old tests, the best `js-test` documentation is its
implementation at
[third_party/WebKit/LayoutTests/resources/js-test.js](../../third_party/WebKit/LayoutTests/resources/js-test.js).

`js-test` tests lean heavily on the Blink-specific `testRunner` testing API.
In a nutshell, the tests call `testRunner.dumpAsText()` to signal that the page
content should be dumped and compared against a text baseline (an
`-expected.txt` file). As a consequence, `js-test` tests are always accompanied
by text baselines. Asynchronous tests also use `testRunner.waitUntilDone()` and
`testRunner.notifyDone()` to tell the testing tools when they are complete.

### Tests that use an HTTP Server

By default, tests are loaded as if via `file:` URLs. Some web platform features
require tests served via HTTP or HTTPS, for example absolute paths (`src=/foo`)
or features restricted to secure protocols.

HTTP tests are those under `LayoutTests/http/tests` (or virtual variants). Use a
locally running HTTP server (Apache) to run them. Tests are served off of ports
8000 and 8080 for HTTP, and 8443 for HTTPS. If you run the tests using
`run-webkit-tests`, the server will be started automatically. To run the server
manually to reproduce or debug a failure:

```bash
cd src/third_party/WebKit/Tools/Scripts
run-blink-httpd start
```

The layout tests will be served from `http://127.0.0.1:8000`. For example, to
run the test `http/tests/serviceworker/chromium/service-worker-allowed.html`,
navigate to
`http://127.0.0.1:8000/serviceworker/chromium/service-worker-allowed.html`. Some
tests will behave differently if you go to 127.0.0.1 instead of localhost, so
use 127.0.0.1.

To kill the server, run `run-blink-httpd --server stop`, or just use `taskkill`
or the Task Manager on Windows, and `killall` or Activity Monitor on MacOS.

The test server sets up an alias to the `LayoutTests/resources` directory. In
HTTP tests, you can access the testing framework at e.g.
`src="/js-test-resources/testharness.js"`.

TODO: Document [wptserve](http://wptserve.readthedocs.io/) when we are in a
position to use it to run layout tests.

## Reference Tests (Reftests)

Reference tests, also known as reftests, perform a pixel-by-pixel comparison
between the rendered image of a test page and the rendered image of a reference
page. Most reference tests pass if the two images match, but there are cases
where it is useful to have a test pass when the two images do _not_ match.

Reference tests are more difficult to debug than JavaScript tests, and tend to
be slower as well. Therefore, they should only be used for functionality that
cannot be covered by JavaScript tests.

New reference tests should follow the
[WPT reftests guidelines](http://testthewebforward.org/docs/reftests.html). The
most important points are summarized below.

* The test page declares the reference page using a `<link rel="match">` or
  `<link rel="mismatch">`, depending on whether the test passes when the test
  image matches or does not match the reference image.
* The reference page must not use the feature being tested. Otherwise, the test
  is meaningless.
* The reference page should be as simple as possible, and should not depend on
  advanced features. Ideally, the reference page should render as intended even
  on browsers with poor CSS support.
* Reference tests should be self-describing.
* Reference tests do _not_ include `testharness.js`.

Our testing infrastructure was designed for the
[WebKit reftests](https://trac.webkit.org/wiki/Writing%20Reftests) that Blink
has inherited. The consequences are summarized below.

* Each reference page must be in the same directory as its associated test.
  Given a test page named `foo` (e.g. `foo.html` or `foo.svg`),
    * The reference page must be named `foo-expected` (e.g.,
      `foo-expected.html`) if the test passes when the two images match.
    * The reference page must be named `foo-expected-mismatch` (e.g.,
      `foo-expected-mismatch.svg`) if the test passes when the two images do
      _not_ match.
* Multiple references and chained references are not supported.

The following example demonstrates a reference test for
[`<ol>`'s reversed attribute](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/ol).
The example assumes that the test page is named `ol-reversed.html`.

```html
<!doctype html>
<meta charset="utf-8">
<link rel="match" href="ol-reversed-expected.html">

<ol reversed>
  <li>A</li>
  <li>B</li>
  <li>C</li>
</ol>
```

The reference page, which must be named `ol-reversed-expected.html`, is below.

```html
<!doctype html>
<meta charset="utf-8">

<ol>
  <li value="3">A</li>
  <li value="2">B</li>
  <li value="1">C</li>
</ol>
```

## Pixel Tests

`testRunner` APIs such as `window.testRunner.dumpAsTextWithPixelResults()` and
`window.testRunner.dumpDragImage()` create an image result that is associated
with the test. The image result is compared against an image baseline, which is
an `-expected.png` file associated with the test, and the test passes if the
image result is identical to the baseline, according to a pixel-by-pixel
comparison. Tests that have image results (and baselines) are called **pixel
tests**.

Pixel tests should still follow the principles laid out above. Pixel tests pose
unique challenges to the desire to have *self-describing* and *cross-platform*
tests. The
[WPT test style guidelines](http://testthewebforward.org/docs/test-style-guidelines.html)
contain useful guidance. The most relevant pieces of advice are below.

* Whenever possible, use a green paragraph / page / square to indicate success.
  If that is not possible, make the test self-describing by including a textual
  description of the desired (passing) outcome.
* Only use the red color or the word `FAIL` to highlight errors. This does not
  apply when testing the color red.
* Use the [Ahem font](https://www.w3.org/Style/CSS/Test/Fonts/Ahem/README) to
  reduce the variance introduced by the platform's text rendering system. This
  does not apply when testing text, text flow, font selection, font fallback,
  font features, or other typographic information.

*** promo
When using `window.testRunner.dumpAsTextWithPixelResults()`, the image result
will always be 800x600px, because test pages are rendered in an 800x600px
viewport. Pixel tests that do not specifically cover scrolling should fit in an
800x600px viewport without creating scrollbars.
***

The following snippet includes the Ahem font in a layout test.

```html
<style>
body {
  font: 10px Ahem;
}
</style>
<script src="/resources/ahem.js"></script>
```

*** promo
Tests outside `LayoutTests/http` and `LayoutTests/imported/wpt` currently need
to use a relative path to
[/third_party/WebKit/LayoutTests/resources/ahem.js](../../third_party/WebKit/LayoutTests/resources/ahem.js)
***

### Tests that need to paint, raster, or draw a frame of intermediate output

A layout test does not actually draw frames of output until the test exits.
Tests that need to generate a painted frame can use
`window.testRunner.displayAsyncThen`, which will run the machinery to put up a
frame, then call the passed callback. There is also a library at
`fast/repaint/resources/text-based-repaint.js` to help with writing paint
invalidation and repaint tests.

## Dump Render Tree (DRT) Tests

A Dump Render Tree test renders a web page and produces up to two results, which
are compared against baseline files:

* All tests output a textual representation of Blink's
  [render tree](https://developers.google.com/web/fundamentals/performance/critical-rendering-path/render-tree-construction),
  which is compared against an `-expected.txt` text baseline.
* Some tests also output the image of the rendered page, which is compared
  against an `-expected.png` image baseline, using the same method as pixel
  tests.

TODO: Document the API used by DRT tests to opt out of producing image results.

A DRT test passes if _all_ of its results match their baselines. Like pixel
tests, the output of DRT tests depends on platform-specific mechanisms, so DRT
tests often require per-platform baselines. Furthermore, DRT tests depend on the
render tree data structure, which means that if we replace the render tree data
structure, we will need to look at each DRT test and consider whether it is
still meaningful.

For these reasons, DRT tests should **only** be used to cover aspects of the
layout code that can only be tested by looking at the render tree. Any
combination of the other test types is preferable to a DRT test. DRT tests are
[inherited from WebKit](https://webkit.org/blog/1456/layout-tests-practice/), so
the repository may have some unfortunate examples of DRT tests.

The following page is an example of a DRT test.

```html
<!doctype html>
<meta charset="utf-8">
<style>
body { font: 10px Ahem; }
span::after {
  content: "pass";
  color: green;
}
</style>
<script src="/resources/ahem.js"></script>

<p><span>Pass if a green PASS appears to the right: </span></p>
```

The most important aspects of the example are that the test page does not
include a testing framework, and that it follows the guidelines for pixel tests.
The test page produces the text result below.

```
layer at (0,0) size 800x600
  LayoutView at (0,0) size 800x600
layer at (0,0) size 800x30
  LayoutBlockFlow {HTML} at (0,0) size 800x30
    LayoutBlockFlow {BODY} at (8,10) size 784x10
      LayoutBlockFlow {P} at (0,0) size 784x10
        LayoutInline {SPAN} at (0,0) size 470x10
          LayoutText {#text} at (0,0) size 430x10
            text run at (0,0) width 430: "Pass if a green PASS appears to the right: "
          LayoutInline {<pseudo:after>} at (0,0) size 40x10 [color=#008000]
            LayoutTextFragment (anonymous) at (430,0) size 40x10
              text run at (430,0) width 40: "pass"
```

Notice that the test result above depends on the size of the `<p>` text. The
test page uses the Ahem font (introduced above), whose main design goal is
consistent cross-platform rendering. Had the test used another font, its text
baseline would have depended on the fonts installed on the testing computer, and
on the platform's font rendering system. Please follow the pixel tests
guidelines and write reliable DRT tests!

WebKit's render tree is described in
[a series of posts](https://webkit.org/blog/114/webcore-rendering-i-the-basics/)
on WebKit's blog. Some of the concepts there still apply to Blink's render tree.

## Directory Structure

The [LayoutTests directory](../../third_party/WebKit/LayoutTests) currently
lacks a strict, formal structure. The following directories have special
meaning:

* The `http/` directory hosts tests that require an HTTP server (see above).
* The `resources/` subdirectory in every directory contains binary files, such
  as media files, and code that is shared by multiple test files.

*** note
Some layout tests consist of a minimal HTML page that references a JavaScript
file in `resources/`. Please do not use this pattern for new tests, as it goes
against the minimality principle. JavaScript and CSS files should only live in
`resources/` if they are shared by at least two test files.
***
