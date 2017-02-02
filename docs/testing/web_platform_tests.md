# Web Platform Tests

The Web Platform Tests project provides a large number of conformance tests for
different aspects of the Web Platform. Currently the tests are hosted on GitHub.
There are two main repositories, one for the CSS Working Group (aka csswg-test),
and one for pretty much everything else (aka web-platform-tests).

There is a plan to merge csswg-test into web-platform-tests, so later, there
will be only one repository.

[TOC]

## Import

Chromium has mirrors
([web-platform-tests](https://chromium.googlesource.com/external/w3c/web-platform-tests/),
[csswg-test](https://chromium.googlesource.com/external/w3c/csswg-test/)) of the
GitHub repos, and periodically imports a subset of the tests so that they are
run as part of the regular Blink layout test testing process.

The goal of this process are to be able to run the Web Platform Tests unmodified
locally just as easily as we can run the Blink tests, and ensure that we are
tracking tip-of-tree in the Web Platform Tests repository as closely as
possible, and running as many of the tests as possible.

### Automatic import process

There is an automatic process for updating the Chromium copy of the Web Platform
Tests. The import is done by the builder [w3c-test-autoroller
builder](https://build.chromium.org/p/chromium.infra.cron/builders/w3c-test-autoroller).

The easiest way to check the status of recent imports is to look at:

-   Recent logs on Buildbot for [w3c-test-autoroller
    builder](https://build.chromium.org/p/chromium.infra.cron/builders/w3c-test-autoroller)
-   Recent CLs created by
    [blink-w3c-test-autoroller@chromium.org](https://codereview.chromium.org/search?owner=blink-w3c-test-autoroller%40chromium.org).

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

If you wish to add more tests (by un-skipping some of the directories currently
skipped in `W3CImportExpectations`), you can modify that file locally and commit
it, and on the next auto-import, the new tests should be imported. If you want
to import immediately, you can also run `wpt-import --allow-local-commits`.

## Contributing tests back to the Web Platform Tests project.

If you need to make changes to Web Platform Tests, just commit your changes
directly to
[LayoutTests/external/wpt](../../third_party/WebKit/LayoutTests/external/wpt)
and the changes will be automatically upstreamed within 24 hours.

Note: if you're adding a new test in `external/wpt`, you'll need to re-generate
MANIFEST.json manually until [CL 2644783003](https://crrev.com/2644783003) is
landed. The command to do so is:

```bash
Tools/Scripts/webkitpy/thirdparty/wpt/wpt/manifest --work \
    --tests-root=LayoutTests/external/wpt
```

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

It's still possible to make direct pull requests to web-platform-tests. The
processes for getting new tests committed the W3C repos are at
http://testthewebforward.org/docs/. Some specifics are at
http://testthewebforward.org/docs/github-101.html.
