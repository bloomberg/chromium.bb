# web-platform-tests

Interoperability between browsers is
[critical](https://www.chromium.org/blink/platform-predictability) to Chromium's
mission of improving the web. We believe that leveraging and contributing to a
shared test suite is one of the most important tools in achieving
interoperability between browsers. The [web-platform-tests
repository](https://github.com/w3c/web-platform-tests) is the primary shared
test suite where all browser engines are collaborating. There's also a
[csswg-test repository](https://github.com/w3c/csswg-test) for CSS tests, but
that will [soon be merged into
web-platform-tests](https://github.com/w3c/csswg-test/issues/1102).

Chromium has 2-way import/export process with the upstream web-platform-tests
repository, where tests are imported into
[LayoutTests/external/wpt](../../third_party/WebKit/LayoutTests/external/wpt)
and any changes to the imported tests are also exported to web-platform-tests.

See http://web-platform-tests.org/ for general documentation on
web-platform-tests, including tips for writing and reviewing tests.

[TOC]

## Importing tests

Chromium has mirrors
([web-platform-tests](https://chromium.googlesource.com/external/w3c/web-platform-tests/),
[csswg-test](https://chromium.googlesource.com/external/w3c/csswg-test/)) of the
GitHub repos, and periodically imports a subset of the tests so that they are
run as part of the regular Blink layout test testing process.

The goal of this process are to be able to run web-platform-tests unmodified
locally just as easily as we can run the Blink tests, and ensure that we are
tracking tip-of-tree in the web-platform-tests repository as closely as
possible, and running as many of the tests as possible.

### Automatic import process

There is an automatic process for updating the Chromium copy of
web-platform-tests. The import is done by the builder [w3c-test-autoroller
builder](https://build.chromium.org/p/chromium.infra.cron/builders/w3c-test-autoroller).

The easiest way to check the status of recent imports is to look at:

-   Recent logs on Buildbot for [w3c-test-autoroller
    builder](https://build.chromium.org/p/chromium.infra.cron/builders/w3c-test-autoroller)
-   Recent CLs created by
    [blink-w3c-test-autoroller@chromium.org](https://codereview.chromium.org/search?owner=blink-w3c-test-autoroller%40chromium.org).

Automatic imports are intended to run at least once every 24 hours.

### Skipped tests

We control which tests are imported via a file called
[W3CImportExpectations](../../third_party/WebKit/LayoutTests/W3CImportExpectations),
which has a list of directories to skip while importing.

In addition to the directories and tests explicitly skipped there, tests may
also be skipped for a couple other reasons, e.g. if the file path is too long
for Windows. To check what files are skipped in import, check the recent logs
for [w3c-test-autoroller
builder](https://build.chromium.org/p/chromium.infra.cron/builders/w3c-test-autoroller).

### Manual import

To pull the latest versions of the tests that are currently being imported, you
can also directly invoke the
[wpt-import](../../third_party/WebKit/Tools/Scripts/wpt-import) script.

That script will pull the latest version of the tests from our mirrors of the
upstream repositories. If any new versions of tests are found, they will be
committed locally to your local repository. You may then upload the changes.

### Enabling import for a new directory

If you wish to add more tests (by un-skipping some of the directories currently
skipped in `W3CImportExpectations`), you can modify that file locally and commit
it, and on the next auto-import, the new tests should be imported.

If you want to import immediately (in order to try the tests out locally, etc)
you can also run `wpt-import --allow-local-commits`, but this is not required.

## Writing tests

To contribute changes to web-platform-tests, just commit your changes directly
to [LayoutTests/external/wpt](../../third_party/WebKit/LayoutTests/external/wpt)
and the changes will be automatically upstreamed within 24 hours.

Changes involving adding, removing or modifying tests can all be upstreamed.
Any changes outside of
[external/wpt](../../third_party/WebKit/LayoutTests/external/wpt) will not be
upstreamed, and any changes `*-expected.txt`, `OWNERS`, and `MANIFEST.json`,
will also not be upstreamed.

Running the layout tests will automatically regenerate MANIFEST.json to pick up
any local modifications.

Most tests are written using testharness.js, see
[Writing Layout Tests](./writing_layout_tests.md) and
[Layout Tests Tips](./layout_tests_tips.md) for general guidelines.

### Write tests against specifications

Tests in web-platform-tests are expected to match behavior defined by the
relevant specification. In other words, all assertions that a test makes
should be derived from a specification's normative requirements, and not go
beyond them. It is often necessary to change the specification to clarify what
is and isn't required.

When the standards discussion is still ongoing or blocked on some implementation
successfully shipping the hoped-for behavior, write the tests outside of
web-platform-tests and upstream them when the specification is finally updated.
Optionally, it may be possible to write deliberately failing tests against the
current specification and later update them.

### Tests that require testing APIs

Tests that depend on `internals.*`, `eventSender.*` or other internal testing
APIs cannot yet be written as part of web-platform-tests.

An alternative is to write manual tests that are automated with scripts from
[wpt_automation](../../third_party/WebKit/LayoutTests/external/wpt_automation).
Such tests still require case-by-case automation to run for other browser
engines, but are more valuable than purely manual tests.

*** note
TODO(foolip): Figure out and document a more scalable test automation solution.
***

### Adding new top-level directories

Entirely new top-level directories should generally be added upstream, since
that's the only way to add an OWNERS file upstream. After adding a new top-level
directory upstream, you should add a line for it in `W3CImportExpectations`.

Adding the new directory (and `W3CImportExpectations` entry) in Chromium and
later adding an OWNERS file upstream also works.

### Will the exported commits be linked to my GitHub profile?

The email you commit with in Chromium will be the author of the commit on
GitHub. You can [add it as a secondary address on your GitHub
account](https://help.github.com/articles/adding-an-email-address-to-your-github-account/)
to link your exported commits to your GitHub profile.

### What if there are conflicts?

This cannot be avoided entirely as the two repositories are independent, but
should be rare with frequent imports and exports. When it does happen, manual
intervention will be needed and in non-trivial cases you may be asked to help
resolve the conflict.

### Direct pull requests

It's still possible to make direct pull requests to web-platform-tests, see
http://web-platform-tests.org/appendix/github-intro.html.

## Reviewing tests

Anyone who can review code and tests in Chromium can also review changes in
[external/wpt](../../third_party/WebKit/LayoutTests/external/wpt)
that will be automatically upstreamed. There will be no additional review in
web-platform-tests as part of the export process.

If upstream reviewers have feedback on the changes, discuss on the pull request
created during export, and if necessary work on a new pull request to iterate
until everyone is satisfied.

When reviewing tests, check that they match the relevant specification, which
may not fully match the implementation. See also
[Write tests against specifications](#Write-tests-against-specifications).
